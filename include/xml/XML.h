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
	bool newLineAfter = false;
	BufArray<SharedString,3> text;

	INLINE ~Item() {
		clear();
	}

	inline void clear();
};

using Content = Array<std::unique_ptr<Item>>;

struct Element : public Item {
	Content content;
	bool selfClosing = false;
	bool newLineInside = false;

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

// This escapes:
//  <   &lt;
//  >   &gt;
//  &   &amp;
//  '   &apos; (if escapeQuotes true)
//  "   &quot; (if escapeQuotes true)
COMMON_LIBRARY_EXPORTED void escapeXMLText(const char* inBegin, const char*const inEnd, Array<char>& outText, bool escapeQuotes);

COMMON_LIBRARY_EXPORTED bool parseTextXML(const char* begin, const char* end, Content& output, bool trimLeadingSpace = true);

COMMON_LIBRARY_EXPORTED bool ReadXMLFile(const char* filename, Content& content, bool trimLeadingSpace = true);

COMMON_LIBRARY_EXPORTED void generateTextXML(const Content& content, Array<char>& output, size_t firstLineTabs, size_t nestingLevel);

COMMON_LIBRARY_EXPORTED bool WriteXMLFile(const char* filename, const Content& content);

} // namespace xml
OUTER_NAMESPACE_END
