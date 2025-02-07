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

#include <libyul/optimiser/LabelIDDispenser.h>

#include <libyul/optimiser/NameCollector.h>
#include <libyul/optimiser/OptimizerUtilities.h>

#include <fmt/compile.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/iota.hpp>

using namespace solidity::yul;

namespace
{
bool isInvalidLabel(
	std::string_view const _label,
	std::set<std::string, std::less<>> const& _reservedLabels,
	Dialect const& _dialect
)
{
	return isRestrictedIdentifier(_dialect, _label) || _reservedLabels.contains(_label);
}
}

LabelIDDispenser::LabelIDDispenser(ASTLabelRegistry const& _labels, std::set<std::string> const& _reservedLabels):
	m_labels(_labels),
	m_reservedLabels(_reservedLabels.begin(), _reservedLabels.end()),
	m_offset(_labels.maximumID() + 1)
{
	m_reservedLabels += std::set{""};
}

LabelIDDispenser::LabelID LabelIDDispenser::newID(LabelID const parent)
{
	m_idToLabelMapping.push_back(resolveBaseID(parent));
	return m_idToLabelMapping.size() - 1 + m_offset;
}

LabelIDDispenser::LabelID LabelIDDispenser::newGhost()
{
	return newID(ASTLabelRegistry::ghostID());
}

LabelIDDispenser::LabelID LabelIDDispenser::resolveBaseID(LabelID _id) const
{
	if (_id >= m_offset && _id != ASTLabelRegistry::ghostID())
		_id = m_idToLabelMapping[_id - m_offset];
	yulAssert(
		_id == ASTLabelRegistry::ghostID() || _id < m_offset,
		"We have at most one level of indirection, this violates this assumption"
	);
	return _id;
}

ASTLabelRegistry LabelIDDispenser::generateNewLabels(Block const&, Dialect const& _dialect) const
{
	// this can be replaced by the actually used ids in the provided block once the AST uses ids instead of YulString
	std::set<LabelID> usedIDs = ranges::views::iota(static_cast<size_t>(0), m_idToLabelMapping.size() + m_offset) | ranges::to<std::set<LabelID>>;

	if (usedIDs.empty())
		return {};

	auto const& originalLabels = m_labels.labels();

	std::vector<uint8_t> reusedLabels (originalLabels.size());
	// this means that everything that is derived from empty / ghost needs to be generated
	reusedLabels[0] = true;

	std::vector<std::string> labels{""};
	labels.reserve(originalLabels.size()+1);
	// this is fine as `usedIds` is guaranteed to be not empty
	std::vector<size_t> idToLabelMap(*std::prev(usedIDs.end()) + 1, 0);
	idToLabelMap[0] = 0;

	std::set<std::string, std::less<>> alreadyDefinedLabels = m_reservedLabels;

	std::vector<LabelID> toGenerate;
	// filter out straightforward case: we just use whatever label was already there and put it into alreadyDefinedLabels
	// otherwise it goes into the toGenerate collection
	for (auto const& id: usedIDs)
	{
		auto const baseID = resolveBaseID(id);
		if (baseID == ASTLabelRegistry::ghostID())
		{
			idToLabelMap[id] = ASTLabelRegistry::ghostID();
			continue;
		}

		auto const baseLabelIndex = m_labels.idToLabelIndex(baseID);
		auto const& baseLabel = originalLabels[baseLabelIndex];
		// if we haven't already reused the label, check that either the id didn't change, then we can just
		// take over the old label, otherwise check that it is a valid label and then reuse
		if (!reusedLabels[baseLabelIndex] && (baseID == id || !isInvalidLabel(baseLabel, m_reservedLabels, _dialect)))
		{
			labels.push_back(baseLabel);
			idToLabelMap[id] = labels.size() - 1;
			alreadyDefinedLabels.insert(baseLabel);
			reusedLabels[baseLabelIndex] = true;
		}
		else
			toGenerate.push_back(id);
	}

	for (auto const& id: toGenerate)
	{
		auto const baseID = resolveBaseID(id);

		// ghost variables get special treatment
		if (baseID == ASTLabelRegistry::ghostID())
		{
			idToLabelMap[id] = ASTLabelRegistry::ghostID();
			continue;
		}

		auto const baseLabelIndex = m_labels.idToLabelIndex(baseID);
		auto const& baseLabel = originalLabels[baseLabelIndex];

		std::string generatedLabel = baseLabel;
		size_t suffix = 1;
		do
		{
			generatedLabel = format(FMT_COMPILE("{}_{}"), baseLabel, suffix++);
		} while (isInvalidLabel(generatedLabel, alreadyDefinedLabels, _dialect));

		labels.push_back(generatedLabel);
		idToLabelMap[id] = labels.size() - 1;
		alreadyDefinedLabels.insert(generatedLabel);
	}

	return ASTLabelRegistry{labels, idToLabelMap};
}
