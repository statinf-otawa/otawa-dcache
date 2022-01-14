/*
 *	AccessesBuilder class interface
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
#ifndef OTAWA_DCACHE_CLPACCESSBUILDER_H_
#define OTAWA_DCACHE_CLPACCESSBUILDER_H_

#include <otawa/hard/Cache.h>
#include <otawa/hard/Memory.h>
#include <otawa/proc/BBProcessor.h>

#include "features.h"

namespace otawa {

namespace clp { class Manager; }	
	
namespace dcache {

using namespace otawa;

class CLPAccessBuilder: public BBProcessor {
public:
	static p::declare reg;
	CLPAccessBuilder(p::declare& r = reg);
	void *interfaceFor(const AbstractFeature &feature) override;

protected:
	void setup(WorkSpace *ws) override;
	void processWorkSpace(WorkSpace *ws) override;
	void processBB(WorkSpace *ws, CFG *g, otawa::Block *b) override;
	void destroyBB (WorkSpace *ws, CFG *cfg, otawa::Block *b) override;
	void destroy(WorkSpace *ws) override;
	void dumpBB(otawa::Block *v, io::Output& out) override;
	
	const hard::Cache *_cache;
	const hard::Memory *_mem;
	SetCollection *_coll;
	FragTable<Access> accs;
	clp::Manager *clp;
};

} }	// otawa::dcache

#endif /* OTAWA_DCACHE_CLPACCESSBUILDER_H_ */
