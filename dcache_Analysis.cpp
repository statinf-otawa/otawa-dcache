/*
 *	Analysis class
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

#include <otawa/dcache/Analysis.h>

namespace otawa { namespace dcache {


/**
 * @class Analysis
 *
 * This class is used to perform analyzes on an instruction cache. It works
 * simply by performing separately the analysis of each set. In addition,
 * it provides facilities to publish the analysis results in the OTAWA
 * framework.
 *
 * For each set,
 * the analysis asks for a domain by calling function Analysis::domainFor().
 * This function is purely virtual and must be overridden to perform
 * a particular analysis.
 *
 * @par Configuration
 *	* @ref ONLY_SET -- select the set to work on (do not process other sets, multiple accepted).
 *
 * @ingroup dcache
 */

///
p::declare Analysis::reg = p::init("otawa::icat::Analysis", Version(1, 0, 0))
	.require(dcache::ACCESS_FEATURE)
	.require(otawa::COLLECTED_CFG_FEATURE);


/**
 * This property is a configuration of Analysis. It selects which sets are processed
 * (multiple combinations accepted).
 */
p::id<int> ONLY_SET("otawa::icat::ONLY_SET");


///
Analysis::Analysis(p::declare& reg): Processor(reg), coll(nullptr), cfgs(nullptr), n(0) { }

///
void Analysis::configure(const PropList& props) {
	Processor::configure(props);
	for(auto s: ONLY_SET.all(props))
		only_sets.add(s);
}

/**
 * @fn ai::Domain *domainFor(const SetCollection *coll, int set);
 * This function is called to obtain the domain  to
 * perform the static analysis for the given cache set. The returned pointer
 * will be fried by the analysis itself.
 *
 * @param coll	Current set collection.
 * @param set	Set to get the domain for.
 * @return		Domain to perform the analysis on the given set.
 */

///
void Analysis::setup(WorkSpace *ws) {

	// get required analyzes
	coll = ACCESS_FEATURE.get(ws);
	ASSERT(coll);
	cfgs = COLLECTED_CFG_FEATURE.get(ws);
	ASSERT(cfgs);
	n = coll->cache().setCount();

	// initialize domains
	doms.set(n, new Domain *[n]);
	for(int i = 0; i < n; i++)
		if(coll->blockCount(i) != 0)
			doms[i] = domainFor(*coll, i);
		else
			doms[i] = nullptr;

	// initialize analyzers
	anas.set(n, new ai::CFGAnalyzer *[n]);
	for(int i = 0; i < n; i++)
		if(doms[i] == nullptr)
			anas[i] = nullptr;
		else
			anas[i] = new ai::CFGAnalyzer(*this, *doms[i]);
}


/**
 * Get the state for the set s before the edge e.
 * Once used, the state must be fried by a call to release().
 * @param e		Looked edge.
 * @param s		Looked set.
 * @return		State in set s before edge e.
 */
ai::State *Analysis::before(Edge *e, int s) {
	auto r = anas[s]->before(e);
	uses.put(r, s);
	return r;
}

/**
 * Get the state for the set s after the edge e.
 * Once used, the state must be fried by a call to release().
 * @param e		Looked edge.
 * @param s		Looked set.
 * @return		State in set s after edge e.
 */
ai::State *Analysis::after(Edge *e, int s) {
	auto r = anas[s]->after(e);
	uses.put(r, s);
	return r;
}

/**
 * Get the state for the set s before the block v.
 * Once used, the state must be fried by a call to release().
 * @param v		Looked block.
 * @param s		Looked set.
 * @return		State in set s before block v.
 */
ai::State *Analysis::before(otawa::Block *v, int s) {
	auto r = anas[s]->before(v);
	uses.put(r, s);
	return r;
}

/**
 * Get the state for the set s after the block v.
 * Once used, the state must be fried by a call to release().
 * @param v		Looked block.
 * @param s		Looked set.
 * @return		State in set s after block v.
 */
ai::State *Analysis::after(otawa::Block *v, int s) {
	auto r = anas[s]->after(v);
	uses.put(r, s);
	return r;
}

/**
 * Get the state before the execution of the access a in the block v for the set
 * touched by a. a must be an access contained in v.
 * The returned state s must be fried by a call to release().
 * @param v		Concerned block.
 * @param a		Looked access.
 * @param set	Set of interest.
 * @return		State before a.
 */
ai::State *Analysis::at(otawa::Block *v, const Access& a, int set) {
	return at(v, a, before(v, set), set);
}

/**
 * Get the state before the execution of the access a along the edge e for the
 * set touched by a. a must be an access contained in the sink of e.
 * The returned state s must be fried by a call to release().
 * @param e		Concerned edge.
 * @param a		Looked access.
 * @param set	Set of interest.
 * @return		State before a.
 */
ai::State *Analysis::at(Edge *e, const Access& a, int set) {
	return at(e->sink(), a, before(e, set), set);
}

///
ai::State *Analysis::at(otawa::Block *v, const Access& a, ai::State *s, int S) {
	auto cs = s;
	for(const auto &b: *ACCESSES(v)) {
		if(&a == &b)
			return cs;
		if(b.access(S)) {
			auto ns = doms[S]->update(a, s);
			if(ns != cs) {
				if(cs != s)
					anas[S]->release(cs);
				uses.remove(cs);
				cs = ns;
				anas[S]->use(cs);
				uses.put(cs, S);
			}
		}
	}
	ASSERTP(false, "access " << a << " not in block " << v);
	return nullptr;
}

/**
 * Free a state previously allocated by one of function before(), after()
 * or at().
 * @param s		State to release.
 */
void Analysis::release(ai::State *s) {
	int S = uses.get(s, -1);
	ASSERT(S >= 0);
	anas[S]->release(s);
}

///
void Analysis::destroy(WorkSpace *ws) {

	// cleanup analyzers
	for(int i = 0; i < n; i++)
		if(anas[i] != nullptr)
			delete anas[i];

	// cleanup domains
	for(int i = 0; i < n; i++)
		if(doms[i] != nullptr)
			delete doms[i];
}

///
void Analysis::process(WorkSpace *ws, int set) {
	if(logFor(LOG_FUN)) {
		log << "\tSET " << set << io::endl;
		if(anas[set] == nullptr)
			log << "\t\tempty\n";
	}
	if(anas[set] != nullptr)
		anas[set]->process();
}

///
void Analysis::dump(WorkSpace *ws, Output& out) {
	if(only_sets)
		for(auto s: only_sets)
			if(s < 0 || s >= coll->setCount())
				log << "ERROR: ignoring invalid set number: " << s << io::endl;
			else
				dump(ws, s, out);
	else
		for(int i = 0; i < n; i++)
			dump(ws, i, out);
}

///
void Analysis::dump(WorkSpace *ws, int set, Output& out) {
	if(anas[set] == nullptr)
		return;
	out << "SET " << set << io::endl;
	for(auto g: *COLLECTED_CFG_FEATURE.get(ws)) {
		out << "\tCFG " << g << io::endl;
		for(auto b: *g) {
			out << "\t\t" << b << ": ";
			auto s = anas[set]->after(b);
			doms[set]->print(s, out);
			anas[set]->release(s);
			out << io::endl;
		}
	}
}


/**
 * Call it collect in garbage collections all states stored in the analysis.
 * @param f		Function to call with each state.
 */
void Analysis::collect(ai::state_collector_t f) {
	for(auto d: doms)
		if(d != nullptr)
			d->collect(f);
	for(auto a: anas)
		if(a != nullptr)
			a->collect(f);
}

///
void Analysis::processWorkSpace(WorkSpace *ws) {
	if(only_sets)
		for(auto s: only_sets)
			if(s < 0 || s >= coll->setCount())
				log << "ERROR: ignoring invalid set number: " << s << io::endl;
			else
				process(ws, s);
	else
		for(int i = 0; i < n; i++)
			if(coll->blockCount(i) != 0)
				process(ws, i);
}

/**
 * @class Domain
 * Domain implementation for states supported by Analysis.
 *
 * @ingroup icat
 */

///
bool Domain::implementsCodePrinting() {
	return true;
}

///
void Domain::printCode(otawa::Block *b, io::Output& out) {
	for(const auto& a: *ACCESSES(b))
		if(a.access(S))
			out << "\t\t" << a << io::endl;;
}

/**
 * Function called for garbage collection to let the domain mark active
 * states. The default implementation does nothing.
 * @param f		Function to call to mark a state is alive.
 */
void Domain::collect(ai::state_collector_t f) {
}

} }	// otawa::dcache


