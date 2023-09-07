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

	bool isEstimating(bool on) const override {
		if(on)
			return !_xs.isEmpty();
		else
			return false;
	}
	
	void estimate(ilp::Constraint *cons, bool on) const override {
		if(on)
			_xs.addLeft(cons);
	}

private:
	const Access& _acc;
	time_t _cost;
	occurrence_t _occ;	
	mutable ilp::Expression _xs;
};


class EventBuilder: public BBProcessor {
public:
	static p::declare reg;
	EventBuilder(p::declare& r = reg):
		BBProcessor(r),
		must(nullptr),
		may(nullptr),
		pers(nullptr),
		mpers(nullptr),
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
		
		// get the cache
		cache = &ACCESS_FEATURE.get(ws)->cache();
	}

	virtual int mustAge(Edge *e, const Access & a, const CacheBlock *cb) {
		return must->age(e, a, cb);
	}
	
	virtual int mpersLevel(Edge *e, const Access& a, const CacheBlock *cb) {
		return mpers->level(e, a, cb);
	}

	virtual int persAge(Edge *e, const Access& a, const CacheBlock *cb) {
		return pers->age(e, a, cb);
	}

	virtual int mayAge(Edge *e, const Access& a, const CacheBlock *cb) {
		return may->age(e, a, cb);
	}

	virtual Block *eventBlock(Edge *e) {
		return e->sink();
	}
	
	virtual void addEvent(Edge *e, Event *evt) {
		EVENT(e).add(evt);
	}
	
	typedef Pair<Event::occurrence_t, Block *> t;
	t classify(Edge *e, const Access& a, const CacheBlock *cb) {

		// AH?
		if(mustAge(e, a, cb) < A)
			return t(Event::NEVER, nullptr);

		// PE?
		else if(mpers != nullptr) {
			Block *h;
			auto n = mpersLevel(e, a, cb);
			if(n != 0) {
				auto l = Loop::of(eventBlock(e));
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
		if(pers != nullptr && persAge(e, a, cb) < A) {
			auto l = Loop::of(eventBlock(e));
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
		if(may != nullptr && mayAge(e, a, cb) >= A)
			return t(Event::NEVER, nullptr);
			
		// NOT-CLASSIFIED
		else
			return t(Event::SOMETIMES, nullptr);
	}

	ot::time worstAccessTime(const Access& a) {
		switch(a.action()) {
		case LOAD:
		case DIRECT_LOAD:		
			return mem->worstReadTime();
		case STORE:
		case DIRECT_STORE:
			return mem->worstWriteTime();
		case NO_ACCESS:
		case PURGE:
			return 0;
		}
		return -1;
	}
	
	Event *processAny(Edge *e, const Access &a) {
		return new Event(
			a,
			worstAccessTime(a),
			Event::SOMETIMES,
			ilp::Expression::null);
	}

	/**
	 * Process a multiple-instruction with T address.
	 * @param e		Current edge.
	 * @param a		Access to build event for.
	 */
	void processMultiTop(Edge *e, const Access& a) {
		if(logFor(LOG_BLOCK))
			log << "\t\t\tusing special multi-access to T at "
				<< a.inst()->address() << io::endl;
		// TO FIX
		auto access_size = sem::size(a.type());
		if(access_size == 0)
			access_size = 4;
		auto size = a.inst()->multiCount() * access_size;
		int cnt = ((size + cache->blockSize() - 1) >> cache->blockBits()) + 1;
		// TO FIX: not really worst case
		ot::time t = worstAccessTime(a);
		for(int i = 0; i < cnt; i++)
			addEvent(e,
				new Event(a, t,	Event::SOMETIMES, ilp::Expression::null));
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
			t = worstAccessTime(a);
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

	/**
	 * Build events for the given access. There is a specific optimization 
	 * for multiple access instruction to T address: as the accesses are considered
	 * as sequential, the number of NC events is: roundup(access size * access/
	 * block size) to the worst case time. Subsequent accesses in the same
	 * instruction can be ignored in this case.
	 * @param e		Current edge.
	 * @param a		Access to build event for.
	 * @return		True if a multiple to T has been managed, false else.
	 */
	bool processAccess(Edge *e, const Access& a) {
		Event *evt = nullptr;

		// build the event
		switch(a.action()) {
		case NO_ACCESS:
			return false;

		case DIRECT_LOAD:
		case DIRECT_STORE:
			evt = processDirect(e, a);
			break;
			
		case PURGE:
			return false;

		case LOAD:
		case STORE:
			switch(a.kind()) {
			case ANY:
				if(a.inst()->isMulti()) {
					processMultiTop(e, a);
					return true;
				}
				else
					evt = processAny(e, a);
				break;
			case BLOCK:
				evt = processBlock(e, a);
				break;
			case ENUM:
				evt = processEnum(e, a);
				break;
			case RANGE:
				evt = processAny(e, a);
				break;
			default:
				ASSERT(false);
				break;
			}
			break;
		}
		
		// add the event
		addEvent(e, evt);
		return false;
	}
	
	void processBB(WorkSpace *ws, CFG *g, Block *b_) override {
		if(!b_->isBasic())
			return;
		auto b = b_->toBasic();

		// set events
		for(auto e: b->inEdges()) {
			Inst *multi = nullptr;
			for(const auto& a: *ACCESSES(b))
				if(a.inst() != multi)
					if(processAccess(e, a))
						multi = a.inst();
		}
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


	AgeInfo *must, *may, *pers;
	MultiAgeInfo *mpers;
	
private:
	const hard::Memory *mem;
	int A;
	int cnt[CAT_CNT];
	ilp::System *sys;
	bool _explicit;
	const hard::Cache *cache;
};


///
p::declare EventBuilder::reg = p::init("otawa::dcache::EventBuilder", Version(1, 0, 0))
	.require(MUST_FEATURE)
	.require(EXTENDED_LOOP_FEATURE)
	.require(hard::MEMORY_FEATURE)
	.require(ipet::ASSIGNED_VARS_FEATURE)
	.require(ACCESS_FEATURE)
	.extend<BBProcessor>()
	.make<EventBuilder>();


///
class PrefixEventBuilder: public EventBuilder {
public:
	static p::declare reg;
	PrefixEventBuilder(p::declare& r = reg): EventBuilder(r), prefix(false) {}

protected:
	
	void addEvent(Edge *e, Event *evt) override {
		if(prefix)
			PREFIX_EVENT(e).add(evt);
		else
			EventBuilder::addEvent(e, evt);
	}
	
	int mustAge(Edge *e, const Access &a, const CacheBlock *cb) override {
		if(prefix)
			return must->age(e->source(), a, cb);
		else
			return EventBuilder::mustAge(e, a, cb);
	}

	int mayAge(Edge *e, const Access &a, const CacheBlock *cb) override {
		if(prefix)
			return may->age(e->source(), a, cb);
		else
			return EventBuilder::mayAge(e, a, cb);
	}

	int persAge(Edge *e, const Access &a, const CacheBlock *cb) override {
		if(prefix)
			return pers->age(e->source(), a, cb);
		else
			return EventBuilder::persAge(e, a, cb);
	}

	int mpersLevel(Edge *e, const Access &a, const CacheBlock *cb) override {
		if(prefix)
			return mpers->level(e->source(), a, cb);
		else
			return EventBuilder::mpersLevel(e, a, cb);
	}
	
	Block *eventBlock(Edge * e) override {
		if(prefix)
			return e->source();
		else
			return EventBuilder::eventBlock(e);
	}

	void processBB(WorkSpace *ws, CFG *g, Block *v) override {
		if(!v->isBasic())
			return;
		auto b = v->toBasic();

		// set events
		prefix = true;
		for(auto e: b->inEdges()) {
			Inst *multi = nullptr;
			for(const auto& a: *ACCESSES(e->source()))
				if(a.inst() != multi)
					if(processAccess(e, a))
						multi = a.inst();
		}
		prefix = false;
		EventBuilder::processBB(ws, g, v);
	}

	void dumpBB(Block *v, io::Output& out) override {
		for(auto e: v->inEdges()) {
			out << "\t\talong " << e << io::endl;
			Vector<otawa::Event *> evts;
			for(auto evt: PREFIX_EVENT.all(e))
				if(dynamic_cast<Event *>(evt) != nullptr)
					evts.add(evt);
			for(int i = evts.length() - 1; i >= 0; i--)
				out << "\t\t[P]" << evts[i]->detail() << io::endl;
			evts.clear();
			for(auto evt: EVENT.all(e))
				if(dynamic_cast<Event *>(evt) != nullptr)
					evts.add(evt);
			for(int i = evts.length() - 1; i >= 0; i--)
				out << "\t\t[B]" << evts[i]->detail() << io::endl;			
		}
	}

	bool prefix;
};


///
p::declare PrefixEventBuilder::reg =
	p::init("otawa::dcache::PrefixEventBuilder", Version(1, 0, 0))
	.extend<EventBuilder>()
	.make<PrefixEventBuilder>()
	.provide(PREFIX_EVENTS_FEATURE);

	
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
p::feature EVENTS_FEATURE("otawa::dcache::EVENTS_FEATURE", p::make<EventBuilder>());


/**
 * Ensure that events generated by the instruction cache analysis, generated by
 * the previous block, are linked to the edge.
 * 
 * Properties:
 *  @ref otawa::PREFIX_EVENT
 * 
 * Default implementation: @ref otawa::dcache::PrefixEventBuilder
 * 
 * @ingroup dcache
 */
p::feature PREFIX_EVENTS_FEATURE("otawa::dcache::PREFIX_EVENTS_FEATURE", p::make<PrefixEventBuilder>());

}}	// otawa::dcache
