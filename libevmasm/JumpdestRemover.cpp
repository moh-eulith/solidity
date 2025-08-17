/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * @author Alex Beregszaszi
 * Removes unused JUMPDESTs.
 */

#include <libevmasm/JumpdestRemover.h>

#include <libevmasm/AssemblyItem.h>

#include <limits>

using namespace solidity;
using namespace solidity::util;
using namespace solidity::evmasm;

bool JumpdestRemover::optimise(std::set<size_t> const& _tagsReferencedFromOutside)
{

	// the code generator can create unnecessary double jumps.
	// calling these "trampolines" to avoid clash with two consequetive jumps (see peephole)
	std::map<size_t, AssemblyItem> trampolines{findTrampolines(m_items, SubAssemblyID{}, _tagsReferencedFromOutside)};

	// Replace all trampoline destinations
	for (size_t i = 0; i < m_items.size(); ++i) {
		if (m_items[i].type() == PushTag)
		{
			auto subAndTag = m_items[i].splitForeignPushTag();
			if (subAndTag.first.empty() && trampolines.count(subAndTag.second))
				m_items[i] = trampolines.at(subAndTag.second);
		}
	}


	std::set<size_t> references{referencedTags(m_items, SubAssemblyID{})};
	references.insert(_tagsReferencedFromOutside.begin(), _tagsReferencedFromOutside.end());

	size_t initialSize = m_items.size();
	/// Remove tags which are never referenced.
	auto pend = remove_if(
		m_items.begin(),
		m_items.end(),
		[&](AssemblyItem const& _item)
		{
			if (_item.type() != Tag)
				return false;
			auto asmIdAndTag = _item.splitForeignPushTag();
			solAssert(asmIdAndTag.first.empty(), "Sub-assembly tag used as label.");
			size_t tag = asmIdAndTag.second;
			return !references.count(tag);
		}
	);
	m_items.erase(pend, m_items.end());
	return m_items.size() != initialSize;
}

std::set<size_t> JumpdestRemover::referencedTags(AssemblyItems const& _items, SubAssemblyID _subId)
{
	std::set<size_t> ret;
	for (auto const& item: _items)
		if (item.type() == PushTag || item.type() == RelativeJump || item.type() == ConditionalRelativeJump)
		{
			auto subAndTag = item.splitForeignPushTag();
			if (subAndTag.first == _subId)
				ret.insert(subAndTag.second);
		}
	return ret;
}

std::map<size_t, AssemblyItem> JumpdestRemover::findTrampolines(AssemblyItems const& _items, SubAssemblyID _subId, std::set<size_t> const& _fromOutside)
{
	std::map<size_t, AssemblyItem> ret;
	static std::set<AssemblyItem> const impassibles { Instruction::RETURN, Instruction::JUMP, Instruction::STOP, Instruction::REVERT };
	TrampolineState state = Start;
	size_t fromTag = 0;
	AssemblyItem toTag = Instruction::REVERT;
	// simple state machine to recognize <impassible> <tag fromTag> <push_tag toTag> <jump>
	for (size_t i = 0; i < _items.size(); ++i)
	{
		auto const& item = _items[i];
		switch (state) {
			case Start:
				if (impassibles.count(item)) state = Impassible;
				break;
			case Impassible:
				if (item.type() == Tag)
				{
					auto subAndTag = item.splitForeignPushTag();
					if (subAndTag.first == _subId && !_fromOutside.count(subAndTag.second))
					{
						fromTag = subAndTag.second;
						state = PlaceTag;
						break;
					}
				}
				--i; // re-examine this item in the next loop iteration
				state = Start;
				break;
			case PlaceTag:
				if (item.type() == PushTag)
				{
					auto subAndTag = item.splitForeignPushTag();
					if (subAndTag.first == _subId)
					{
						toTag = item.pushTag();  // help! it's unclear whether the debug info should be from fromTag or toTag
						state = PushTagAfterTag;
						break;
					}
				}
				--i;
				state = Start;
				break;
			case PushTagAfterTag:
				if (item == Instruction::JUMP)
					ret.insert({fromTag, toTag});
				else --i;
				state = Start;
				break;
		}
	}

	return ret;
}
