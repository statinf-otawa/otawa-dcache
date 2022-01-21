/*
 *	EventBuilder class
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2021, IRIT UPS.
 *
 *	OTAWA is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	OTAWA is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with OTAWA; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <otawa/cfg/features.h>
#include <otawa/cfg/Loop.h>
#include <otawa/events/features.h>
#include <otawa/hard/Memory.h>
#include <otawa/ipet/features.h>
#include <otawa/ipet/IPET.h>
#include <otawa/ilp/Constraint.h>
#include <otawa/proc/BBProcessor.h>

#include "otawa/dcache/features.h"

namespace otawa { namespace dcache { 

/**
 * Instruction cache event.
 */
class Event: public otawa::Event {
public:
	
	Event(
		const Access& a,
	   ot::time c,
	   occurrence_t o,
	   const ilp::Expression& xs = ilp::Expression::null
	):
		otawa::Event(a.inst()),
		_acc(a),
		_cost(c),
		_occ(o),
		_xs(xs)
		{ }

	cstring name() const override { return "DC"; }
	string detail() const override {
		StringBuffer buf;
		buf << name() << ": " << _acc << " - " << _occ;
		if(_occ == SOMETIMES) {
			if(_xs.count() > 0)
				buf << " (xe <= " << _xs << ")";
			else
				buf << " (no bound)";
		}
		return buf.toString();
	}

	kind_t kind() const override { return MEM; }
	ot::time cost() const override { return _cost; }
	occurrence_t occurrence() const override { return _occ; }
	inline const Access& access() const { return _acc; }

	type_t type() const override { return LOCAL; }

	bool isEstimating(bool on) override {
		if(on)
			return !_xs.isEmpty();
		else
			return false;
	}
	
	void estimate(ilp::Constraint *cons, bool on) override {
		if(on)
			_xs.addRight(cons);
	}

private:
	const Access& _acc;
	time_t _cost;
	occurrence_t _occ;	
	ilp::Expression _xs;
};


class EventBuilder: public BBProcessor {
public:
	static p::declare reg;
	EventBuilder(p::declare& r = reg):
		BBProcessor(r),
		must(nullptr),
		may(nullptr),
		pers(nullptr),
		//mpers(nullptr),
		mem(nullptr),
		A(0)
	{
		array::set(cnt, CAT_CNT, 0);
	}

	void configure(const PropList& props) override {
		BBProcessor::configure(props);
		_explicit = ipet::EXPLICIT(props);
	}
	
protected:

	void setup(WorkSpace *ws) override {

		// get MUST analysis
		must = MUST_FEATURE.get(ws);
		ASSERT(must);
		A = must->wayCount();

		// get MAY analysis
		if(ws->provides(MAY_FEATURE)) {
			may = MAY_FEATURE.get(ws);
			ASSERT(may != nullptr);
		}

		// get PERS analysis
		if(ws->provides(PERS_FEATURE)) {
			pers = PERS_FEATURE.get(ws);
			ASSERT(pers != nullptr);
		}

		// get multi-PERS analysis
		if(ws->provides(MULTI_PERS_FEATURE)) {
			mpers = MULTI_PERS_FEATURE.get(ws);
			ASSERT(mpers != nullptr);
		}
		
		// get the memory
		mem = hard::MEMORY_FEATURE.get(ws);
		
		// get the system
		sys = ipet::SYSTEM(ws);
	}

	typedef Pair<Event::occurrence_t, Block *> t;
	t classify(Edge *e, const Access& a, const CacheBlock *cb) {

		// AH?
		if(must->age(e, a, cb) < A)
			return t(Event::NEVER, nullptr);

		// PE?
		else if(mpers != nullptr) {
			Block *h;
			auto n = mpers->level(e, a, cb);
			if(n != 0) {
				auto l = Loop::of(e->sink());
				for(int i = 1; i < n; i++) {
					if(!l->isTop())
						l = l->parent();
					else if(e->sink()->cfg()->callCount() == 1)
						l = Loop::of(*e->sink()->cfg()->callers().begin());
					else
						break;
				}
				if(l->isTop())
					h = l->cfg()->entry()->outEdges().begin()->sink();
				else
					h = l->header();
				return t(Event::SOMETIMES, h);
			}
		}
			
		// PE?
		if(pers != nullptr && pers->age(e, a, cb) < A) {
			auto l = Loop::of(e->sink());
			if(!l->isTop())
				while(!l->parent()->isTop())
					l = l->parent();
			Block *h;
			if(l->isTop())
				h = l->cfg()->entry()->outEdges().begin()->sink();
			else
				h = l->header();
			return t(Event::SOMETIMES, h);
		}
			
		// AM?
		if(may != nullptr && may->age(e, a, cb) >= A)
			return t(Event::NEVER, nullptr);
			
		// NOT-CLASSIFIED
		else
			return t(Event::SOMETIMES, nullptr);
	}

	Event *processAny(Edge *e, const Access &a) {
		ot::time t;
		if(a.action() == LOAD)
			t = mem->worstReadTime();
		else
			t = mem->worstWriteTime();
		return new Event(a, t, Event::SOMETIMES, ilp::Expression::null);
	}
	

	Event *processBlock(Edge *e, const Access& a) {
		auto cb = a.block();
		auto bank = cb->bank();
		auto c = classify(e, a, cb);
		ot::time t;
		if(a.action() == LOAD)
			t = bank->readLatency();
		else
			t = bank->writeLatency();
		ilp::Expression xs;
		if(c.snd != nullptr)
			xs.add(1., ipet::VAR(c.snd));
		return new Event(a, t, c.fst, xs);
	}
	
	Event *processEnum(Edge *e, const Access& a) {
		
		// prepare the data
		auto bank = a.blocks()[0]->bank();
		ot::time t;
		if(a.action() == LOAD)
			t = bank->readLatency();
		else
			t = bank->writeLatency();
		
		// prepare according to all blocks
		Event::occurrence_t o = Event::NO_OCCURRENCE;
		ilp::Expression xs;
		for(auto cb: a.blocks()) {
			auto c = classify(e, a, cb);
			o = o | c.fst;
			if(c.snd != nullptr)
				xs.add(1., ipet::VAR(c.snd));
			else if(c.fst == Event::SOMETIMES)
				return processAny(e, a);
		}
		
		// build the event
		return new Event(a, t, o, xs);
	}
	
	Event *processDirect(Edge *e, const Access& a) {
		ot::time t = 0;
		const hard::Bank *bank;
		switch(a.kind()) {
		case ANY:
		case RANGE:
			t = a.action() == DIRECT_LOAD ? mem->worstReadTime() : mem->worstWriteTime();
			break;
		case BLOCK:
			bank = a.block()->bank();
			t = a.action() == DIRECT_LOAD ? bank->readLatency() : bank->writeLatency();
			break;
		case ENUM:
			bank = a.blocks()[0]->bank();
			t = a.action() == DIRECT_LOAD ? bank->readLatency() : bank->writeLatency();
			break;
		default:
			ASSERT(false);
			break;
		}
		return new Event(a, t, Event::ALWAYS);
	}
	
	void processAccess(Edge *e, const Access& a) {
		Event *evt = nullptr;

		// build the event
		switch(a.action()) {
		case NO_ACCESS:
			return;

		case DIRECT_LOAD:
		case DIRECT_STORE:
			evt = processDirect(e, a);
			break;
			
		case PURGE:
			return;

		case LOAD:
		case STORE:
			switch(a.kind()) {
			case ANY:	evt = processAny(e, a); break;
			case BLOCK:	evt = processBlock(e, a); break;
			case ENUM:	evt = processEnum(e, a); break;
			case RANGE:	evt = processAny(e, a); break;
			default:	ASSERT(false); break;
			}
			break;
		}
		
		// add the event
		EVENT(e).add(evt);
	}
	
	void processBB(WorkSpace *ws, CFG *g, Block *b_) override {
		if(!b_->isBasic())
			return;
		auto b = b_->toBasic();

		// set events
		for(const auto& a: *ACCESSES(b))
			for(auto e: b->inEdges())
				processAccess(e, a);
	}
	
	void dumpBB(Block *v, io::Output& out) override {
		for(auto e: v->inEdges()) {
			out << "\t\talong " << e << io::endl;
			Vector<otawa::Event *> evts;
			for(auto evt: EVENT.all(e))
				if(dynamic_cast<Event *>(evt) != nullptr)
					evts.add(evt);
			for(int i = evts.length() - 1; i >= 0; i--)
				out << "\t\t" << evts[i]->detail() << io::endl;
		}
	}

private:

	AgeInfo *must, *may, *pers;
	MultiAgeInfo *mpers;
	const hard::Memory *mem;
	int A;
	int cnt[CAT_CNT];
	ilp::System *sys;
	bool _explicit;
};


///
p::declare EventBuilder::reg = p::init("otawa::dcache::EventBuilder", Version(1, 0, 0))
	.require(MUST_FEATURE)
	.require(EXTENDED_LOOP_FEATURE)
	.require(hard::MEMORY_FEATURE)
	.require(ipet::ASSIGNED_VARS_FEATURE)
	//.use(MAY_FEATURE)
	//.use(PERS_FEATURE)
	//.use(MULTI_PERS_FEATURE)
	//.provide(CATEGORY_FEATURE)
	.extend<BBProcessor>()
	.make<EventBuilder>();


/**
 * Ensure that events generated by the instruction cache analysis are linked
 * to the edge.
 * 
 * Properties:
 *  @ref otawa::EVENT
 * 
 * Default implementation: @ref otawa::dcache::EventBuilder
 * 
 * @ingroup dcache
 */
p::feature EVENT_FEATURE("otawa::dcache::EVENT_FEATURE", p::make<EventBuilder>());

}}	// otawa::dcache
