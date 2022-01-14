/*
 *	dcache::CLPBlockBuilder class implementation
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2013, IRIT UPS.
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

#include <otawa/hard/CacheConfiguration.h>
#include <otawa/hard/Cache.h>
#include <otawa/hard/Memory.h>
#include <otawa/hard/Platform.h>
#include <otawa/prog/WorkSpace.h>
#include <otawa/prog/Process.h>
#include <otawa/dcache/CLPBlockBuilder.h>
#include <otawa/clp/features.h>

namespace otawa { namespace dcache {

/**
 * @class CLPBlockBuilder
 * Build the list of blocks used for L1 data cache analysis and decorate each basic block
 * with the list of performed accesses, based on a CLP analysis
 *
 * @p Provided Features
 * @li @ref DATA_BLOCK_FEATURE
 *
 * @p Required Features
 * @li @ref
 *
 * @p Configuration
 * @li @ref INITIAL_SP
 * @ingroup dcache
 */
p::declare CLPBlockBuilder::reg = p::init("otawa::dcache::CLPBlockBuilder", Version(1, 0, 0))
	.base(BBProcessor::reg)
	.maker<CLPBlockBuilder>()
	.provide(DATA_BLOCK_FEATURE)
	.provide(CLP_BLOCK_FEATURE)
	.require(otawa::clp::ANALYSIS_FEATURE)
	.require(hard::CACHE_CONFIGURATION_FEATURE)
	.require(hard::MEMORY_FEATURE);


/**
 * This feature ensures that accessed data blocks have been built for data cache analysis based
 * on the reference obtaine from CLP analysis.
 *
 * @par Default Processor
 * @li @ref CLPBlockBuilder
 *
 * @see clp
 * @ingroup dcache
 */
p::feature CLP_BLOCK_FEATURE("otawa::dcache::CLP_BLOCK_FEATURE", new Maker<CLPBlockBuilder>());


/**
 */
CLPBlockBuilder::CLPBlockBuilder(p::declare& r): BBProcessor(r), cache(0), mem(0), colls(0), man(0) {
}


/**
 */
void CLPBlockBuilder::setup(WorkSpace *ws) {

	// get cache
	auto conf = hard::CACHE_CONFIGURATION_FEATURE.get(ws);
	if(conf == nullptr)
		throw otawa::Exception("no data cache!");
	cache = conf->dataCache();
	if(cache == nullptr)
		throw otawa::Exception("no data cache!");
	if(cache->replacementPolicy() != hard::Cache::LRU)
		throw otawa::Exception("unsupported replacement policy in data cache !");

	// get memory
	mem = hard::MEMORY_FEATURE.get(ws);

	// build the block collection
	colls = new BlockCollection[cache->rowCount()];
	DATA_BLOCK_COLLECTION(ws) = colls;
	for(int i = 0; i < cache->rowCount(); i++)
		colls[i].setSet(i);

	// allocate the manager
	man = clp::ANALYSIS_FEATURE.get(ws);
}


/**
 */
void CLPBlockBuilder::cleanup(WorkSpace *ws) {
}


/**
 */
void CLPBlockBuilder::processBB(WorkSpace *ws, CFG *cfg, otawa::Block *b) {
	if(!b->isBasic())
		return;
	BasicBlock *bb = b->toBasic();
	clp::ObservedState *s = nullptr;
	sem::Block buf;
	auto action = BlockAccess::NONE;

	for(auto bundle: bb->bundles()) {
		buf.clear();
		bundle.semInsts(buf);
		
		for(int i = 0; i < buf.length(); i++) {

			// scan the instruction
			switch(buf[i].op) {
			case sem::LOAD:
				action = BlockAccess::LOAD;
				break;
			case sem::STORE:
				action = BlockAccess::STORE;
				break;
			default:
				continue;
			}
			
			// add the access
			s = man->at(bb, bundle.first(), i, s);
			auto addr = man->valueOf(s, buf[i].addr());
			if(logFor(LOG_INST))
				log << "\t\t\t" << bundle.address() << ": " << i << ": "
					<< " access to " << addr << io::endl;

			// access to T
			if(addr.isAll())
				accs.add(BlockAccess(bundle.first(), action));

			// constant access
			else if(addr.isConst()) {
				auto l = addr.lower(); // the actual address
				const hard::Bank *bank = mem->get(l);
				if(bank == nullptr)
					throw otawa::Exception(_ << "no memory bank for address "
						<< Address(l)
						<< " accessed from " << bundle.address());
				if(!bank->isCached()) {
					ncaccs.add(NonCachedAccess(bundle.first(), action, Address(l)));
					if(logFor(LOG_INST))
						log << "\t\t\t" << action << " at " << bundle.address() << " is not cached!\n";
					continue;
				}
				const Block& block = colls[cache->set(l)].obtain(cache->round(l));
				accs.add(BlockAccess(bundle.first(), action, block));
			}

			// range too big
			else if(addr.isInf()
			|| cache->countBlocks(addr.start(), addr.stop()) > cache->setCount())
				accs.add(BlockAccess(bundle.first(), action));
			
			// range access
			else {
				auto l = addr.start(), h = addr.stop();
				const hard::Bank *bank = mem->get(l);
				if(bank == nullptr)
					throw otawa::Exception(_ << "no memory bank for address "
						<< Address(l) << " accessed from " << bundle.address());
							else if(bank != mem->get(h)) {
								warn(_ << "access at " << bundle.address() << " spanning over several banks considered as any.\n");
								accs.add(BlockAccess(bundle.first(), action));
							}
				else if(bank != mem->get(h)) {
					warn(_ << "access at " << bundle.address()
						<< " spans over several bank: considered access to T");
					accs.add(BlockAccess(bundle.first(), action));
					continue;
				}
				else if(!bank->isCached()) {
					ncaccs.add(NonCachedAccess(bundle.first(), action, l));
					if(logFor(LOG_INST))
						log << "\t\t\t" << action << " at " << bundle.address()
							<< " is not cached!\n";
					continue;
				}
				else if(cache->block(l) == cache->block(h)) {
					const Block& block = colls[cache->set(l)].obtain(cache->round(l));
						accs.add(BlockAccess(bundle.first(), action, block));
				}
				else {
					Vector<const Block *> bs;
					for(auto a = cache->round(l); true; a = a + cache->blockSize()) {
						auto& block = colls[cache->set(a)].obtain(a);
						bs.add(&block);
						if(a == cache->round(h))
							break;
					}
					accs.add(BlockAccess(bundle.first(), action, bs, cache->setCount()));
				}
			}
		}
	}
	if(s != nullptr)
		man->release(s);
	
	// record the accesses
	BlockAccess *tab = new BlockAccess[accs.length()];
	for(int i = 0; i < accs.count(); i++) {
		tab[i] = accs[i];
		if(logFor(LOG_BB))
			log << "\t\t\tBlockAccess:" << tab[i] << io::endl;
	}
	DATA_BLOCKS(bb) = pair(accs.count(), tab);
	accs.clear();

	NonCachedAccess *nctab = new NonCachedAccess[ncaccs.length()];
	for(int i = 0; i < ncaccs.count(); i++) {
		nctab[i] = ncaccs[i];
		if(logFor(LOG_BB))
			log << "\t\t\tNonCachedAccess:" << nctab[i] << io::endl;
	}
	NC_DATA_ACCESSES(bb) = pair(ncaccs.count(), nctab);
	ncaccs.clear();
}

///
void CLPBlockBuilder::dumpBB(otawa::Block *v, io::Output& out) {
	if(!v->isBasic())
		return;
	auto bb = v->toBasic();
	
	// print cache accesses
	const auto& accs = *DATA_BLOCKS(bb);
	for(auto a: accs)
		out << "\t\t\t" << a << io::endl;
	
	// print non-cache accesses
	auto naccs = *NC_DATA_ACCESSES(bb);
	for(int i = 0; i < naccs.fst; i++)
		out << "\t\t\t" <<  naccs.snd[i] << io::endl;
}

} }	// otawa::dcache
