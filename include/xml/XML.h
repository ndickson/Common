#pragma once

// This file contains declarations of functions for reading and writing
// arbitrary data from XML files into a simple in-memory format.
// Definitions of the functions are in XML.cpp

#include "../Array.h"
#include "../ArrayDef.h"
#include "../SharedString.h"
#include "../SharedStringDef.h"
#include "../Types.h"

#include <memory>

OUTER_NAMESPACE_BEGIN
namespace xml {

using namespace COMMON_LIBRARY_NAMESPACE;

enum class ItemType
{
	// <elementName name="value">content</elementName>
	// <elementName name="value"/>
	//
	// Each element has one piece of text for the elementName,
	// and two pieces of text for each attribute, in the Item.
	// Whitespace between will be skipped.
	// This is the only subclass of Item, containing an array of Items.
	ELEMENT,

	// Text, ideally without ampersands or less than signs.
	TEXT,

	// &entityRef; or &#charRef;
	//
	// DTDs have %parameterEntityRef; too, but this doesn't apply
	// to regular XML documents, so this code doesn't bother.
	//
	// The text between & and ; is the only text in the Item.
	ENTITY,

	// <![CDATA[Literal character text]]>
	//
	// The text between <![CDATA[ and ]]> is the only text in the Item.
	CDATA,

	// <!--Comment-->
	//
	// The text between <!-- and --> is the only text in the Item.
	COMMENT,

	// <?ProcessingInstructionName Info text?>
	//
	// The Item will contain a piece of text for ProcessingInstructionName,
	// and another for the info text, skipping any whitespace between
	// the two.
	PROCESSING_INSTRUCTION,

	// <!DECLARATION Info text>
	//
	// The Item will contain a piece of text for DECLARATION,
	// and another for the info text, skipping any whitespace between
	// the two.
	DECLARATION
};

struct Item {
	ItemType type;
	BufArray<SharedString,3> text;
	bool selfClosing;
	bool block;

	INLINE ~Item() {
		clear();
	}

	inline void clear();
};

using Content = Array<std::unique_ptr<Item>>;

struct Element : public Item {
	Content content;

	inline void clear() {
		// This is used for destruction, so content must have any allocation freed.
		content.setCapacity(0);
		text.setCapacity(0);
	}
};

inline void Item::clear() {
	if (type == ItemType::ELEMENT) {
		static_cast<Item*>(this)->clear();
	}
	else {
		text.setCapacity(0);
	}
}

COMMON_LIBRARY_EXPORTED bool ReadXMLFile(const char* filename, Content& content);

COMMON_LIBRARY_EXPORTED bool WriteXMLFile(const char* filename, const Content& content, bool binary = false);

COMMON_LIBRARY_EXPORTED const char* parseTextXML(const char* begin, const char* end, Content& output);
COMMON_LIBRARY_EXPORTED void generateTextXML(const Content& value, Array<char>& output, size_t firstLineTabs, size_t nestingLevel);


} // namespace xml
OUTER_NAMESPACE_END
