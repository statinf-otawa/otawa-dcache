/*
 *	icat plugin hook
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

#include <otawa/hard/Cache.h>
#include <otawa/hard/Memory.h>
#include <otawa/proc/ProcessorPlugin.h>
#include <otawa/program.h>

#include <otawa/dcache/features.h>

using namespace elm;
using namespace otawa;

namespace otawa { namespace dcache {

/**
 * @defgroup dcache	Data Cache Analysis by Categories
 * This plug-in provides all what needed to perform data cache analysis by category.
 */


/// plug-in descriptor
class Plugin: public ProcessorPlugin {
public:
	Plugin(void): ProcessorPlugin("otawa::dcache", Version(1, 0, 0), OTAWA_PROC_VERSION) { }
};

} } // otawa::icat

otawa::dcache::Plugin otawa_dcache;
ELM_PLUGIN(otawa_dcache, OTAWA_PROC_HOOK);
