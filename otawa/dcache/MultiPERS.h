/*
 *	MultiPERS Domain interface
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
#ifndef OTAWA_DCACHE_MULTIPERS_H_
#define OTAWA_DCACHE_MULTIPERS_H_

#include <elm/avl/Map.h>
#include "PERS.h"

namespace otawa { namespace dcache {

class MultiACS: public GCState {
public:
	MultiACS(int D, ACS *i = nullptr);
	MultiACS(MultiACS *a);
	MultiACS(MultiACS *a, int D, ACS *i = nullptr);
	MultiACS(MultiACS *a, int OD, int ND, ACS *i = nullptr);
	void mark(AbstractGC& gc) override;
	AllocArray<ACS *> as;
};

class MultiPERS: public Domain {
public:

	MultiPERS(const SetCollection& coll, int set, int assoc, ListGC& gc);

	ai::State *bot() override;
	ai::State *top() override;
	ai::State *entry() override;
	bool equals(ai::State *s1, ai::State *s2) override;
	ai::State *join(ai::State *s1, ai::State *s2) override;

	ai::State *update(Edge *e, ai::State *s) override;

	bool implementsPrinting() override;
	void print(ai::State *s, io::Output& out) override;

	bool implementsIO() override;
	void save(ai::State *s, io::OutStream *out) override;
	ai::State *load(io::InStream *in) override;

	ai::State *update(const Access& a, ai::State *s) override;
	void collect(ai::state_collector_t f) override;

private:
	PERS pers;
	ListGC& gc;
	MultiACS *BOT, *TOP, *os;
	avl::Map<Block *, int> ds;
	inline MultiACS *make(int D, ACS *I)
		{ return new(gc.alloc<MultiACS>()) MultiACS(D, I); }
	inline MultiACS *copy(MultiACS *a)
		{ return new(gc.alloc<MultiACS>()) MultiACS(a); }
	inline MultiACS *copy(MultiACS *a, int D, ACS *i)
		{ return new(gc.alloc<MultiACS>()) MultiACS(a, D, i); }
	inline MultiACS *copy(MultiACS *a, int OD, int ND, ACS *i)
		{ return new(gc.alloc<MultiACS>()) MultiACS(a, OD, ND, i); }
};

} }		// otawa::dcache

#endif /* OTAWA_DCACHE_MULTIPERS_H_ */
