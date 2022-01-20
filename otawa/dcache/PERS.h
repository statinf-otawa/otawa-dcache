/*
 *	PERS Domain interface
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
#ifndef OTAWA_DCACHE_PERS_H_
#define OTAWA_DCACHE_PERS_H_

#include <elm/int.h>
#include "features.h"
#include "ACS.h"

namespace otawa { namespace dcache {

class PERS: public ACSDomain {
public:

	PERS(const SetCollection& coll, int set, int assoc, ListGC& gc);

	ai::State *entry() override;
	void collect(ai::state_collector_t f) override;

	ai::State *join(ai::State *s1, ai::State *s2) override;
	ai::State *update(Edge *e, ai::State *s) override;
	ai::State *update(const Access& a, ai::State *s) override;

	ACS *access(ACS *s, int b) const;
	ACS *purge(ACS *s, int b) const;
	ACS *accessAny(ACS *s) const;

	inline ACS *empty() const { return EMPTY; }

private:
	ACS *EMPTY;
};

} }		// otawa::dcache

#endif /* OTAWA_DCACHE_MUST_H_ */
