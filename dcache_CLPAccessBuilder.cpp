/*
 *	AccessesBuilder class implementation
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2020, IRIT UPS.
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

#include <elm/io/StringOutput.h>

#include <otawa/hard/CacheConfiguration.h>
#include <otawa/clp/features.h>
#include <otawa/prog/Process.h>
#include <otawa/sem/inst.h>

#include "otawa/dcache/CLPAccessBuilder.h"

namespace otawa { namespace dcache {

static action_t asDirect(action_t a) {
	switch(a) {
	case LOAD:	return DIRECT_LOAD;
	case STORE:	return DIRECT_STORE;
	default:	return a;
	}
}

	
/**
 * @class CLPAccessesBuilder
 * Processor building the instruction cache accesses of the current program.
 *
 * Provided features:
 * * @ref otawa::dcache::ACCESS_FEATURE
 * * @ref otawa::dcache::CLP_ACCESS_FEATURE .
 *
 * Extends BBProcessor.
 *
 * Required features encompass
 * * @ref otawa::hard::CACHE_CONFIGURATION_FEATURE
 * * @ref otawa::hard::MEMORY_FEATURE
 * * @ref otawa::clp::ANALYSIS_FEATURE
 *
 * @ingroup dcache
 */

///
p::declare CLPAccessBuilder::reg =
	p::init("otawa::dcache::CLPAccessBuilder", Version(1, 0, 0))
	.make<CLPAccessBuilder>()
	.require(hard::CACHE_CONFIGURATION_FEATURE)
	.require(hard::MEMORY_FEATURE)
	.require(clp::ANALYSIS_FEATURE)
	.provide(ACCESS_FEATURE)
	.provide(CLP_ACCESS_FEATURE)
	.extend<BBProcessor>();

///
CLPAccessBuilder::CLPAccessBuilder(p::declare& r):
	BBProcessor(r),
	_cache(nullptr),
	_mem(nullptr),
	_coll(nullptr),
	clp(nullptr)
	{ }

///
void *CLPAccessBuilder::interfaceFor(const AbstractFeature &feature) {
	if(&feature == &ACCESS_FEATURE || &feature == &CLP_ACCESS_FEATURE)
		return _coll;
	else
		return nullptr;
}

///
void CLPAccessBuilder::setup(WorkSpace *ws) {
	const hard::CacheConfiguration *conf = hard::CACHE_CONFIGURATION_FEATURE.get(ws);
	if(conf == nullptr || conf->dataCache() == nullptr)
		throw ProcessorException(*this, "no data cache!");
	else {
		_cache = conf->dataCache();
		_mem = hard::MEMORY_FEATURE.get(ws);
		ASSERTP(_mem != nullptr, "no memory defined");
		_coll = new SetCollection(*_cache, *_mem);
		clp = clp::ANALYSIS_FEATURE.get(ws);
	}
}

///
void CLPAccessBuilder::processBB(WorkSpace *ws, CFG *g, otawa::Block *b) {
	if(_cache == nullptr || !b->isBasic())
		return;
	BasicBlock *bb = b->toBasic();
	clp::ObservedState *s = nullptr;
	sem::Block buf;
	int f = accs.length();

	for(auto inst: *bb) {
		buf.clear();
		inst->semInsts(buf);
		
		for(int i = 0; i < buf.length(); i++) {

			// compute the action
			auto action = NO_ACCESS;
			switch(buf[i].op) {
			case sem::LOAD:
				action = LOAD;
				break;
			case sem::STORE:
				action = STORE;
				break;
			default:
				continue;
			}
			
			// get the accessed address
			s = clp->at(bb, inst, i, s);
			auto addr = clp->valueOf(s, buf[i].addr());
			if(logFor(LOG_INST))
				log << "\t\t\t" << inst->address() << ": " << i << ": "
					<< " access to " << addr << io::endl;

			// access to T
			if(addr.isAll())
				accs.add(Access(inst, action, buf[i].type(), buf[i].memIndex()));

			// constant access
			else if(addr.isConst()) {
				auto a = addr.lower(); // the actual address
				auto b = _coll->add(a);
				if(b == nullptr)
					throw otawa::Exception(_ << "no memory bank for address "
						<< Address(a) << " accessed from " << instInfo(inst) << '.');
				if(action == STORE && !_cache->doesWriteAllocate())
					action = asDirect(action);
				else if(b->id() < 0) {
					if(logFor(LOG_INST))
						log << "\t\t\t" << action << " at " << inst->address()
							<< " is not cached!\n";
					action = asDirect(action);
				}
				accs.add(Access(inst, action, b, buf[i].type(), buf[i].memIndex()));
			}

			// range too big
			else if(addr.isInf()
			|| _cache->countBlocks(addr.start(), addr.stop()) >= (unsigned)_cache->setCount()){
				accs.add(Access(inst, action, buf[i].type(), buf[i].memIndex()));
            }
			
			// range access
			else {
				auto l = addr.start(), h = addr.stop();
				auto lb = _coll->add(l), hb = _coll->add(h);
				if(lb == nullptr || hb == nullptr)
					throw otawa::Exception(_ << "no memory bank for address "
						<< Address(l) << " accessed from " << inst->address());
				else if(lb->bank() != hb->bank()) {
					warn(_ << "access at " << inst->address()
						<< " spanning over several banks considered as T.\n");
					accs.add(Access(inst, action));
				}
				else if(!lb->bank()->isCached()) {
					action = asDirect(action);
					if(logFor(LOG_INST))
						log << "\t\t\t" << action << " at " << inst->address()
							<< " is not cached!\n";
				}
				if(action == STORE && !_cache->doesWriteAllocate())
					action = asDirect(action);
				if(lb == hb)
					accs.add(Access(inst, action, lb, buf[i].type(), buf[i].memIndex()));
				else {
					Vector<const CacheBlock *> bs;
					for(auto a = _cache->round(l); true;
					a = a + _cache->blockSize()) {
						auto block = _coll->add(a);
						bs.add(block);
						if(a == _cache->round(h))
							break;
					}
					accs.add(Access(inst, action, bs, buf[i].type(), buf[i].memIndex()));
				}
			}
		}
	}
	if(s != nullptr)
		clp->release(s);

	// record cached access
	ACCESSES(b) = AccessList(accs, f, accs.length() - f);
}

///
void CLPAccessBuilder::destroyBB (WorkSpace *ws, CFG *cfg, otawa::Block *b) {
	if(!b->isBasic())
		return;
	ACCESSES(b).remove();
}

///
void CLPAccessBuilder::destroy(WorkSpace *ws) {
	if(_cache != nullptr)
		BBProcessor::destroy(ws);
	if(_coll != nullptr)
		delete _coll;
}

///
void CLPAccessBuilder::processWorkSpace(WorkSpace *ws) {
	if(_cache != nullptr)
		BBProcessor::processWorkSpace(ws);
}

///
void CLPAccessBuilder::dumpBB(otawa::Block *v, io::Output& out) {
	for(const auto& a: *ACCESSES(v))
		out << "\t\t" << a << io::endl;
}

/**
 * This feature is a specialization of @ref otawa::dcache::ACCESS_FEATURE using
 * CLPs to determine the addresses.
 * 
 * Processors:
 * * @ref otawa::dcache::CLPAccessBuilder
 * 
 * @ingroup dcache
 */
p::interfaced_feature<const SetCollection> CLP_ACCESS_FEATURE(
	"otawa::dcache::CLP_ACCESS_FEATURE",
	p::make<CLPAccessBuilder>()
);

} }	// otawa::dcache
