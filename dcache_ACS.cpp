/*
 *	ACS and ACSDomain implementation
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

#include "otawa/dcache/ACS.h"

namespace otawa { namespace dcache {

/**
 * @class ACS
 * Represents an abstract cache state.
 * @ingroup dcache
 */

/**
 * @fn ACS::ACS(int N);
 * Build a non-initialized ACS.
 * @param N		Cache block count.
 */

/**
 * @fn ACS::ACS(int N, age_t a);
 * Build an ACS initialized with the given age.
 * @param N		Cache block count.
 * @param a		Initialization age.
 */

/**
 * @fn ACS::ACS(int N, const ACS& a);
 * Build an ACS by copying an existing one.
 * @param N	Cache block count.
 * @param a	ACS to copy.
 */

/**
 * Print the ACS.
 * @param coll	Collection of cache blocks.
 * @param set	Used set.
 * @param out	Output stream.
 */
void ACS::print(const dcache::SetCollection& coll, int set, io::Output& out) {
	out << "{ ";
	for(int i = 0; i < coll.blockCount(set); i++) {
		if(i != 0)
			out << ", ";
		out << coll.address(coll.block(set, i)) << ": ";
		if(age[i] == BOT)
			out << "_";
		else
			out << age[i];
	}
	out << " }";
}

/**
 * Save the ACS to the stream.
 * @param N		Cache block count.
 * @param out	Output stream.
 */
void ACS::save(int N, io::OutStream *out) {
	int size = N * sizeof(ACS::age_t);
	if(out->write(reinterpret_cast<const char *>(age), size) != size)
		throw io::IOException(out->lastErrorMessage());
}

/**
 * Load the content of the ACS from the given input stream.
 * @param N		Cache block count.
 * @param in	Input stream.
 */
void ACS::load(int N, io::InStream *in) {
	int size = N * sizeof(ACS::age_t);
	if(in->read(age, size) != size)
		throw io::IOException(in->lastErrorMessage());
}

/**
 * Compare lexicographically the current ACS with the given one.
 * @param N	Cache block count.
 * @param a	ACS to compare with.
 * @return	0 for equality, <0 if current ACS is less than the given, >0 else.
 */
bool ACS::equals(int N, ACS *a) {
	return array::equals(age, a->age, N);
}


///
void ACS::mark(AbstractGC& gc) {
	gc.mark(this, sizeof(ACS));
}


/**
 * @class ACSDomain
 * A domain providing basic services to manage ACS.
 * @ingroup dcache
 */

///
ACSDomain::ACSDomain(const SetCollection& collection, int set, int assoc, int top, ListGC& gc_):
	Domain(set),
	coll(collection),
	gc(gc_),
	N(coll.blockCount(set)),
	A(assoc),
	sumA(assoc * collection.blockCount(set)),
	BOT(make(ACS::BOT)),
	TOP(make(top)),
	os(nullptr)
{
	ASSERT(A > 0);
}


///
ai::State *ACSDomain::bot() {
	return BOT;
}

///
ai::State *ACSDomain::top() {
	return TOP;
}

///
ai::State *ACSDomain::entry() {
	return TOP;
}

///
bool ACSDomain::equals(ai::State *_1, ai::State *_2) {
	auto s1 = acs(_1), s2 = acs(_2);
	if(s1 == BOT || s2 == BOT)
		return s1 == s2;
	else
		return s1->equals(N, s2);
}

///
ai::State *ACSDomain::update(Edge *e, ai::State *s) {
	return s;
}

///
bool ACSDomain::implementsPrinting() {
	return true;
}

///
void ACSDomain::print(ai::State *s, io::Output& out) {
	if(s == TOP)
		out << "T";
	else if(s == BOT)
		out << "_";
	else
		acs(s)->print(coll, S, out);
}

///
bool ACSDomain::implementsIO() {
	return true;
}

///
void ACSDomain::save(ai::State *s, io::OutStream *out) {
	acs(s)->save(N, out);
}

ai::State *ACSDomain::load(io::InStream *in) {
	auto s = new(gc.alloc<ACS>()) ACS(N);
	s->load(N, in);
	return s;
}

///
void ACSDomain::collect(ai::state_collector_t f) {
	if(os != nullptr)
		f(os);
	f(BOT);
	f(TOP);
}

} }	// otawa::dcache
