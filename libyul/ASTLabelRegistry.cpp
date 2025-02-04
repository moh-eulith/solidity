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

#include <libyul/ASTLabelRegistry.h>

#include <libyul/Exceptions.h>

#include <fmt/format.h>

#include <range/v3/algorithm/all_of.hpp>
#include <range/v3/algorithm/max.hpp>
#include <range/v3/view/map.hpp>

using namespace solidity::yul;

namespace
{

bool isInGhostFormat(std::string_view const _label)
{
	using namespace std::literals::string_view_literals;
	static size_t constexpr prefixLength = "GHOST["sv.length();
	static size_t constexpr postfixLength = "]"sv.length();
	if (_label.size() < prefixLength + postfixLength + 1)
		return false;
	if (!_label.starts_with("GHOST[") || !_label.ends_with("]"))
		return false;

	auto const maybeID = _label.substr(prefixLength, _label.size() - prefixLength - postfixLength);
	return ranges::all_of(maybeID, isdigit);
}

}

ASTLabelRegistry::ASTLabelRegistry(): m_labels{""}, m_idToLabelMapping{0} {}

ASTLabelRegistry::ASTLabelRegistry(std::vector<std::string> _labels, std::vector<size_t> _idToLabelMapping)
{
	yulAssert(!_labels.empty());
	yulAssert(_labels[0].empty());
	yulAssert(!_idToLabelMapping.empty());
	yulAssert(_idToLabelMapping[0] == 0);
	// using vector<uint8_t> over vector<bool>, as the latter is optimized for space-efficiency
	std::vector<uint8_t> labelVisited (_labels.size(), false);
	size_t numLabels = 0;
	for (auto const& labelIndex: _idToLabelMapping)
	{
		if (labelIndex == ghostID())
			continue;
		yulAssert(labelIndex < _labels.size());
		// it is possible to have multiple references to empty / ghost
		yulAssert(
			labelIndex == 0 || !labelVisited[labelIndex],
			fmt::format("LabelID {} (label \"{}\") is not unique.", labelIndex, _labels[labelIndex])
		);
		labelVisited[labelIndex] = true;
		yulAssert(!isInGhostFormat(_labels[labelIndex]), "Labels of the form GHOST[id] are reserved.");
		if (labelIndex >= 1)
			++numLabels;
	}
	yulAssert(numLabels + 1 == _labels.size(), "Unused labels present.");
	m_labels = std::move(_labels);
	m_idToLabelMapping = std::move(_idToLabelMapping);
}

ASTLabelRegistry::LabelID ASTLabelRegistry::maximumID() const
{
	yulAssert(!m_idToLabelMapping.empty());
	return m_idToLabelMapping.size() - 1;
}

size_t ASTLabelRegistry::idToLabelIndex(LabelID const _id) const
{
	yulAssert(_id < m_idToLabelMapping.size());
	return m_idToLabelMapping[_id];
}

std::string_view ASTLabelRegistry::operator[](LabelID const _id) const
{
	auto const labelIndex = idToLabelIndex(_id);
	if (labelIndex == ghostID())
		return lookupGhost(_id);
	return m_labels[labelIndex];
}

std::optional<ASTLabelRegistry::LabelID> ASTLabelRegistry::findIdForLabel(std::string_view const _label) const {
	if (_label.empty())
		return emptyID();
	for (LabelID id = 1; id <= maximumID(); ++id)
		if ((*this)[id] == _label)
			return id;
	return std::nullopt;
}

std::string_view ASTLabelRegistry::lookupGhost(LabelID const _id) const
{
	yulAssert(idToLabelIndex(_id) == ghostID());
	auto const [it, _] = m_ghostLabelCache.try_emplace(_id, fmt::format("GHOST[{}]", _id));
	return it->second;
}

ASTLabelRegistryBuilder::DefinedLabels::DefinedLabels():
	m_mapping{{"", 0}}
{}

std::tuple<ASTLabelRegistry::LabelID, bool> ASTLabelRegistryBuilder::DefinedLabels::tryInsert(
	std::string_view const _label,
	ASTLabelRegistry::LabelID const _id
)
{
	auto const [it, emplaced] = m_mapping.try_emplace(std::string{_label}, _id);
	return std::make_tuple(it->second, emplaced);
}

ASTLabelRegistryBuilder::ASTLabelRegistryBuilder():
	m_nextID(1)
{}

ASTLabelRegistryBuilder::ASTLabelRegistryBuilder(ASTLabelRegistry const& _existingRegistry)
{
	yulAssert(!_existingRegistry.labels().empty() && _existingRegistry[0].empty());
	auto const maxId = _existingRegistry.maximumID();
	for (size_t i = 1; i <= maxId; ++i)
	{
		auto const existingLabel = _existingRegistry[i];
		if (!existingLabel.empty())
		{
			if (_existingRegistry.idToLabelIndex(i) == ASTLabelRegistry::ghostID())
				m_ghosts.push_back(i);
			else
			{
				auto const [_, inserted] = m_definedLabels.tryInsert(_existingRegistry[i], i);
				yulAssert(inserted);
			}
		}
	}
	m_nextID = _existingRegistry.maximumID() + 1;
}

ASTLabelRegistry::LabelID ASTLabelRegistryBuilder::define(std::string_view const _label)
{
	yulAssert(!isInGhostFormat(_label));
	auto const [id, inserted] = m_definedLabels.tryInsert(_label, m_nextID);
	if (inserted)
		m_nextID++;
	return id;
}

ASTLabelRegistry::LabelID ASTLabelRegistryBuilder::addGhost()
{
	m_ghosts.push_back(m_nextID);
	return m_nextID++;
}

ASTLabelRegistry ASTLabelRegistryBuilder::build() const
{
	auto const& labelToIDMapping = m_definedLabels.labelToIDMapping();
	yulAssert(labelToIDMapping.contains(""));
	yulAssert(labelToIDMapping.at("") == 0);

	std::vector<std::string> labels{""};
	labels.reserve(labelToIDMapping.size());
	auto const maxLabelId = ranges::max(labelToIDMapping | ranges::views::values);
	auto const maxGhostId = m_ghosts.empty() ? 0 : m_ghosts.back();
	std::vector<size_t> idToLabelMapping( std::max(maxLabelId, maxGhostId) + 1, 0);
	yulAssert(!idToLabelMapping.empty(), "Mapping must at least contain empty label");
	for (auto const& [label, id]: labelToIDMapping)
	{
		// skip empty and ghost
		if (id < 1)
			continue;

		labels.emplace_back(label);
		idToLabelMapping[id] = labels.size() - 1;
	}
	for (auto const ghostId: m_ghosts)
		idToLabelMapping[ghostId] = ASTLabelRegistry::ghostID();
	return ASTLabelRegistry{std::move(labels), std::move(idToLabelMapping)};
}
