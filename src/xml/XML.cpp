// This file contains definitions of functions for reading and writing
// arbitrary data from XML files into a simple in-memory format.
// Declarations of the functions are in XML.h

#include "xml/XML.h"
#include "ArrayDef.h"
#include "File.h"
#include "text/TextFunctions.h"

OUTER_NAMESPACE_BEGIN
namespace xml {

void escapeXMLText(const char* inBegin, const char* const inEnd, Array<char>& outText, bool escapeQuotes) {
	const char*const lt = "&lt;";
	const char*const gt = "&gt;";
	const char*const amp = "&amp;";
	const char*const apos = "&apos;";
	const char*const quot = "&quot;";

	for (const char* text = inBegin; text != inEnd && *text != 0; ++text) {
		const char c = *text;
		const char* escapeString = nullptr;
		size_t escapeSize;
		if (c == '<') {
			escapeString = lt;
			escapeSize = 4;
		}
		else if (c == '>') {
			escapeString = gt;
			escapeSize = 4;
		}
		else if (c == '&') {
			escapeString = amp;
			escapeSize = 5;
		}
		else if (escapeQuotes) {
			if (c == '\'') {
				escapeString = apos;
				escapeSize = 6;
			}
			else if (c == '\"') {
				escapeString = quot;
				escapeSize = 6;
			}
		}

		if (escapeString == nullptr) {
			// Not escaped, so just the one character.
			outText.append(c);
			continue;
		}

		// Copy the escape string into outText.
		size_t textBeginIndex = outText.size();
		outText.setSize(textBeginIndex + escapeSize);
		char* textBegin = &(outText[textBeginIndex]);
		for (size_t i = 0; i != escapeSize; ++i) {
			textBegin[i] = escapeString[i];
		}
	}
}

// begin should be pointing to just after a '<' character.
// If begin points to a valid tag name, this returns the number of bytes in that tag name,
// and fills in the type.
// Possible types are ELEMENT (e.g. "name" or "/name"), PROCESSING_INSTRUCTION (e.g. "?name"),
// CDATA (e.g. "![CDATA["), COMMENT (e.g. "!--"), and DECLARATION (e.g. "!NAME").
//
// If none of those, this is not a valid tag name, type is set to TEXT, and zero is returned.
static size_t tryParseTagName(const char* begin, const char* end, ItemType& type) {
	const char* text = begin;
	if (text == end || *text == 0) {
		type = ItemType::TEXT;
		return 0;
	}

	// The first character needs special handling.
	char c = *text;

	if (c == '?') {
		// Processing instruction
		type = ItemType::PROCESSING_INSTRUCTION;
		++text;
	}
	else if (c == '!') {
		// Comment or CDATA or declaration
		type = ItemType::DECLARATION;
		++text;
		if (text + 2 < end || end == nullptr) {
			if ((text[0] == '-') && (text[1] == '-')) {
				type = ItemType::COMMENT;
				return 3;
			}
			if (text + 7 < end || end == nullptr) {
				if ((text[0] == '[') && (text[1] == 'C') && (text[2] == 'D') && (text[3] == 'A') &&
					(text[4] == 'T') && (text[5] == 'A') && (text[6] == '[')
				) {
					type = ItemType::CDATA;
					return 8;
				}

			}
		}
	}
	else if (c == '/') {
		// End tag
		type = ItemType::ELEMENT;
		++text;
	}
	else {
		// Start tag
		type = ItemType::ELEMENT;
	}

	if (text == end || *text == 0) {
		type = ItemType::TEXT;
		return 0;
	}

	c = *text;
	if (c <= ' ' || c == '<' || c == '>' || c == '=' || c == '&') {
		// Not a valid tag, so treat the < character as text.
		type = ItemType::TEXT;
		return 0;
	}

	// The first character is valid, so go until either the end,
	// or a whitespace character, or an invalid character.
	++text;
	while (text != end && *text != 0) {
		c = *text;
		// '>' is for a tag with no attributes.
		// '/' is for a self-closing tag with no attributes.
		// '?' is for a processing instruction end.
		if (c <= ' ' || c == '>' || c == '/' || (type == ItemType::PROCESSING_INSTRUCTION && c == '?')) {
			// Valid tag
			return text - begin;
		}
		if (c == '<' || c == '=' || c == '&') {
			// Invalid tag
			type = ItemType::TEXT;
			return 0;
		}
	}
	return text - begin;
}

static size_t tryParseEntityName(const char* begin, const char* end) {
	const char* text = begin;
	while (text != end && *text != 0) {
		char c = *text;
		if (c <= ' ' || c == '<' || c == '>' || c == '=' || c == '&' || c == '/') {
			// Invalid character for an entity, so assume the '&' was just text.
			return 0;
		}
		if (c == ';') {
			return text - begin;
		}
	}
	// No semi-colon before the end of the text, so not an entity.
	return 0;
}

static bool handleNewLine(const char*& text, const char*const end) {
	if (text == end || (*text != '\r' && *text != '\n')) {
		// Not a new line.
		return false;
	}

	// Skip the LF, CR, or CRLF.
	bool isCR = (*text == '\r');
	++text;
	if (text != end && isCR && (*text == '\n')) {
		++text;
	}

	// Skip leading whitespace on the new line.
	while (text != end && *text != 0 && *text <= ' ') {
		++text;
	}

	return true;
}

bool parseTextXML(const char* begin, const char* end, Content& output, bool trimLeadingSpace) {
	const char* text = begin;
	const char* currentTextStart = text;
	Content* currentContent = &output;
	BufArray<Element*,16> elementStack;
	while (text != end && *text != 0) {
		char c = *text;
		if (c != '<' && c != '&') {
			if (trimLeadingSpace && (c == '\r' || c == '\n')) {
				// Insert completed text block, regardless of whether it's empty,
				// to handle the new line.
				std::unique_ptr<Item> item(new Item());
				item->type = ItemType::TEXT;
				item->text.append(SharedString(currentTextStart, text-currentTextStart));
				item->newLineAfter = handleNewLine(text, end);
				currentContent->append(std::move(item));
			}
			else {
				++text;
			}
			continue;
		}

		if (c == '&') {
			++text;
			size_t nameLength = tryParseEntityName(text, end);

			if (nameLength == 0) {
				continue;
			}

			if (text != currentTextStart + 1) {
				// Insert completed text block.
				std::unique_ptr<Item> item(new Item());
				item->type = ItemType::TEXT;
				item->text.append(SharedString(currentTextStart, text-1-currentTextStart));
				currentContent->append(std::move(item));
			}

			std::unique_ptr<Item> item(new Item());
			item->type = ItemType::ENTITY;
			const char* entityBegin = text;
			text += nameLength;
			item->text.append(SharedString(entityBegin, nameLength));

			assert(*text == ';');
			++text;

			if (trimLeadingSpace) {
				item->newLineAfter = handleNewLine(text, end);
			}

			currentContent->append(std::move(item));
			currentTextStart = text;

			continue;
		}

		assert(c == '<');
		++text;
		ItemType type;
		size_t nameLength = tryParseTagName(text, end, type);
		if (type == ItemType::TEXT) {
			continue;
		}
		if (text != currentTextStart + 1) {
			// Insert completed text block.
			std::unique_ptr<Item> item(new Item());
			item->type = ItemType::TEXT;
			item->text.append(SharedString(currentTextStart, text-1-currentTextStart));
			currentContent->append(std::move(item));
		}
		if (type == ItemType::COMMENT || type == ItemType::CDATA) {
			// Skip over "!--" or "![CDATA["
			text += nameLength;

			// Search for "-->" or "]]>"
			const char searchChar = (type == ItemType::COMMENT) ? '-' : ']';
			const char*const commentStart = text;
			size_t numSearchChars = 0;
			bool finishedComment = false;
			while (text != end && *text != 0) {
				c = *text;
				if (c == searchChar) {
					if (numSearchChars < 2) {
						++numSearchChars;
					}
				}
				else if (c == '>' && numSearchChars == 2) {
					++text;
					std::unique_ptr<Item> item(new Item());
					item->type = type;
					item->text.append(SharedString(commentStart, text-commentStart-3));

					if (trimLeadingSpace) {
						item->newLineAfter = handleNewLine(text, end);
					}

					currentContent->append(std::move(item));
					finishedComment = true;
					break;
				}
				else {
					numSearchChars = 0;
				}
			}
			if (!finishedComment) {
				std::unique_ptr<Item> item(new Item());
				item->type = type;
				item->text.append(SharedString(commentStart, text-commentStart));
				currentContent->append(std::move(item));
			}
		}
		else if (type == ItemType::DECLARATION || type == ItemType::PROCESSING_INSTRUCTION) {
			std::unique_ptr<Item> item(new Item());
			item->type = type;
			// Skip the '!' or '?' character.
			item->text.append(SharedString(text + 1, nameLength - 1));
			// Skip the name.
			text += nameLength;

			// Skip the first span of whitespace.
			text = text::skipWhitespace(text, end);

			// Find ">" or "?>".
			const char*const otherTextBegin = text;
			while (text != end && *text != 0 && *text != '>' && (type != ItemType::PROCESSING_INSTRUCTION || *text != '?')) {
				++text;
			}
			const char*const otherTextEnd = text;
			if (otherTextEnd != otherTextBegin) {
				item->text.append(SharedString(otherTextBegin, otherTextEnd-otherTextBegin));
			}

			if (type == ItemType::PROCESSING_INSTRUCTION && text != end && *text == '?') {
				++text;
				// Skip until '>'
				while (text != end && *text != 0 && *text != '>') {
					++text;
				}
			}
			if (text != end && *text == '>') {
				++text;
			}

			if (trimLeadingSpace) {
				item->newLineAfter = handleNewLine(text, end);
			}

			currentContent->append(std::move(item));
		}
		else { // if (type == ItemType::ELEMENT)
			const bool endTag = (*text == '/');
			if (endTag) {
				ShallowString name(text+1, nameLength-1);
				text += nameLength;

				// Find the end of the end tag, in case there's extra data to be skipped.
				while (text != end) {
					char c = *text;
					if (c == 0 || c == '>') {
						// Skip the '>', but don't skip a null terminator.
						text += (c == '>');
						break;
					}
					++text;
				}

				// Search backward for a matching tag in elementStack.
				size_t iPlusOne = elementStack.size();
				for ( ; iPlusOne > 0; --iPlusOne) {
					if (elementStack[iPlusOne-1]->text[0] == name) {
						break;
					}
				}
				if (iPlusOne == 0) {
					// There's no matching start tag, so treat this as a self-closing element tag
					// with no attributes, instead.

					std::unique_ptr<Item> item(new Element());
					item->text.append(SharedString(name));
					if (trimLeadingSpace) {
						item->newLineAfter = handleNewLine(text, end);
					}
					currentContent->append(std::move(item));
				}
				else {
					// Close every element after and including iPlusOne-1.

					if (iPlusOne < elementStack.size()) {
						Element& dest = *elementStack[iPlusOne-1];

						// Any elements after iPlusOne-1 should be changed to be empty and self-closing.
						// These are invalid, but self-closing is the safest guess.
						for (size_t elementi = iPlusOne; elementi < elementStack.size(); ++elementi) {
							Element& source = *elementStack[elementi];
							// Move everything inside source out to dest.
							for (size_t i = 0, size = source.content.size(); i != size; ++i) {
								dest.content.append(std::move(source.content[i]));
							}
							// Clear source's content and mark it as self-closing.
							source.content.setCapacity(0);
							source.selfClosing = true;
							source.newLineAfter = source.newLineInside;
							source.newLineInside = false;
						}

						if (trimLeadingSpace) {
							elementStack[iPlusOne-1]->newLineAfter = handleNewLine(text, end);
						}
						elementStack.setSize(iPlusOne-1);
					}
					else {
						// The element at iPlusOne-1 can be closed as is.
						if (trimLeadingSpace) {
							elementStack.last()->newLineAfter = handleNewLine(text, end);
						}
						elementStack.removeLast();
					}

					if (elementStack.size() == 0) {
						currentContent = &output;
					}
					else {
						currentContent = &(elementStack.last()->content);
					}
				}
			}
			else {
				// Start tag
				std::unique_ptr<Element> item(new Element());
				item->text.append(SharedString(text, nameLength));
				text += nameLength;

				// Find tag attributes.
				text = text::skipWhitespace(text, end);
				while (text != end && *text != 0 && *text != '/' && *text != '>') {
					const char*const attributeNameBegin = text;
					// Find the end of the tag attribute name.
					while (text != end && *text != 0 && *text > ' ' && *text != '=' && *text != '/' && *text != '>') {
						++text;
					}
					const char*const attributeNameEnd = text;
					item->text.append(SharedString(attributeNameBegin, attributeNameEnd-attributeNameBegin));
					item->text.append(SharedString());
					text = text::skipWhitespace(text, end);
					if (text != end && *text == '=') {
						// Skip over the '=' that should be here and any whitespace after that shouldn't.
						++text;
						text = text::skipWhitespace(text, end);
					}
					if (text == end || *text == 0 || *text == '/' || *text == '>') {
						// Attribute with no value at end of tag, so break.
						break;
					}
					char c = *text;
					if (c =='\"' || c == '\'') {
						// Quoted attribute value string.  Note that there's no C/JSON-style
						// quote escaping here, since entities should be used to represent
						// quotes, ampersands, and less-than signs.
						++text;
						const char quote = c;
						const char*const attributeValueBegin = text;
						while (text != end && *text != 0 && *text != c) {
							++text;
						}
						const char*const attributeValueEnd = text;
						item->text.last() = SharedString(attributeValueBegin, attributeValueEnd-attributeValueBegin);
						if (text != end && *text != 0) {
							++text;
						}
					}
					else {
						// Unquoted attribute value string.  Go until whitespace character,
						// '/', or '>'.
						const char*const attributeValueBegin = text;
						while (text != end && *text != 0 && *text > ' ' && *text != '/' && *text != '>') {
							++text;
						}
						const char*const attributeValueEnd = text;
						item->text.last() = SharedString(attributeValueBegin, attributeValueEnd-attributeValueBegin);
					}
					text = text::skipWhitespace(text, end);
				}

				// Check if self-closing.
				bool selfClosing = (text != end && *text == '/');
				item->selfClosing = selfClosing;
				if (selfClosing) {
					// Skip to the '>' and just past it.
					while (text != end && *text != '>') {
						++text;
					}
					if (text != end && *text == '>') {
						++text;
					}
				}
				else if (text != end) {
					assert(*text == '>');
					++text;
				}

				if (trimLeadingSpace) {
					bool hadNewLine = handleNewLine(text, end);
					if (selfClosing) {
						item->newLineAfter = hadNewLine;
					}
					else {
						item->newLineInside = hadNewLine;
					}
				}

				if (!selfClosing) {
					elementStack.append(item.get());
				}
				Content* innerContent = &(item->content);
				currentContent->append(std::move(item));
				if (!selfClosing) {
					currentContent = innerContent;
				}
			}
		}

		currentTextStart = text;
	}

	if (text != currentTextStart + 1) {
		// Insert completed text block.
		std::unique_ptr<Item> item(new Item());
		item->type = ItemType::TEXT;
		item->text.append(SharedString(currentTextStart, text-currentTextStart));
		currentContent->append(std::move(item));
	}
	if (elementStack.size() != 0) {
		// Any elements left unclosed should be changed to be empty and self-closing.
		// These are invalid, but self-closing is the safest guess.
		for (size_t elementi = 0; elementi < elementStack.size(); ++elementi) {
			Element& source = *elementStack[elementi];
			// Move everything inside source out to output.
			for (size_t i = 0, size = source.content.size(); i != size; ++i) {
				output.append(std::move(source.content[i]));
			}
			// Clear source's content and mark it as self-closing.
			source.content.setCapacity(0);
			source.selfClosing = true;
		}
		elementStack.setSize(0);
	}

	return true;
}

bool ReadXMLFile(const char* filename, Content& content, bool trimLeadingSpace) {
	Array<char> contents;
	bool success = ReadWholeFile(filename, contents);
	if (!success || contents.size() == 0) {
		return false;
	}

	size_t size = contents.size();
	const char* data = contents.data();
	const char* end = data+size;
	success = parseTextXML(data, end, content, trimLeadingSpace);
	return success;
}

void generateTextXML(const Content& content, Array<char>& output, size_t firstLineTabs, size_t nestingLevel) {
	size_t numTabs = firstLineTabs;
	for (size_t contenti = 0, contentSize = content.size(); contenti != contentSize; ++contenti) {
		if (numTabs != 0) {
			size_t tabBegin = output.size();
			output.setSize(tabBegin + numTabs);
			for (size_t i = 0; i != numTabs; ++i) {
				output[tabBegin+i] = '\t';
			}
			numTabs = 0;
		}

		const char* beforeText = nullptr;
		size_t beforeTextSize = 0;
		const char* afterText = nullptr;
		size_t afterTextSize = 0;
		const char*const beforeAfterTexts[][2] = {
			{"<!--", "-->"},
			{"<![CDATA[", "]]>"},
			{"&", ";"}
		};
		const size_t beforeAfterTextSizes[][2] = {
			{4, 3},
			{9, 3},
			{1, 1}
		};

		bool isSingleCase = true;
		size_t textBeginIndex = output.size();
		const Item& item = *content[contenti];
		const char* text = item.text[0].data();
		switch (item.type) {
			case ItemType::TEXT: {
				// TODO: Escape text if needed!
				break;
			}
			case ItemType::COMMENT: {
				beforeText = beforeAfterTexts[0][0];
				beforeTextSize = beforeAfterTextSizes[0][0];
				afterText = beforeAfterTexts[0][1];
				afterTextSize = beforeAfterTextSizes[0][1];
				break;
			}
			case ItemType::CDATA: {
				beforeText = beforeAfterTexts[1][0];
				beforeTextSize = beforeAfterTextSizes[1][0];
				afterText = beforeAfterTexts[1][1];
				afterTextSize = beforeAfterTextSizes[1][1];
				break;
			}
			case ItemType::ENTITY: {
				beforeText = beforeAfterTexts[2][0];
				beforeTextSize = beforeAfterTextSizes[2][0];
				afterText = beforeAfterTexts[2][1];
				afterTextSize = beforeAfterTextSizes[2][1];
				break;
			}
			case ItemType::ELEMENT:
			case ItemType::PROCESSING_INSTRUCTION:
			case ItemType::DECLARATION: {
				isSingleCase = false;
				// Add start tag.
				output.append('<');
				if (item.type != ItemType::ELEMENT) {
					char c = (item.type == ItemType::DECLARATION) ? '!' : '?';
					output.append(c);
				}

				size_t textSize = item.text[0].size();
				textBeginIndex = output.size();
				output.setSize(textBeginIndex + textSize);
				char* textBegin = &(output[textBeginIndex]);
				for (size_t i = 0; i != textSize; ++i) {
					textBegin[i] = text[i];
				}

				// Add attributes to start tag.
				for (size_t i = 0, numTexts = item.text.size()-1; i != numTexts; ++i) {
					const SharedString& string = item.text[i+1];
					bool odd = (i & 1) != 0;
					if (!odd) {
						output.append(' ');
					}
					else {
						output.append('=');
						output.append('\"');
					}

					size_t textSize = string.size();
					if (textSize != 0) {
						textBeginIndex = output.size();
						output.setSize(textBeginIndex + textSize);
						const char* attributeText = string.data();
						char* textBegin = &(output[textBeginIndex]);
						for (size_t i = 0; i != textSize; ++i) {
							textBegin[i] = attributeText[i];
						}
					}

					if (odd) {
						output.append('\"');
					}
				}

				const Element* element = (item.type == ItemType::ELEMENT) ? static_cast<const Element*>(&item) : nullptr;

				// Complete the start tag.
				if (element != nullptr && element->selfClosing) {
					output.append('/');
				}
				else if (item.type == ItemType::PROCESSING_INSTRUCTION) {
					output.append('?');
				}
				output.append('>');

				if (element != nullptr && !element->selfClosing) {
					if (element->newLineInside) {
						numTabs = nestingLevel + (element->content.size() != 0);
						output.append('\n');
					}

					if (element->content.size() != 0) {
						generateTextXML(element->content, output, numTabs, nestingLevel+1);
						if (element->content.last()->newLineAfter) {
							numTabs = nestingLevel;
						}
						else {
							numTabs = 0;
						}
					}

					// Add end tag indent.
					if (numTabs != 0) {
						size_t tabBegin = output.size();
						output.setSize(tabBegin + numTabs);
						for (size_t i = 0; i != numTabs; ++i) {
							output[tabBegin+i] = '\t';
						}
						numTabs = 0;
					}

					// Add end tag.
					textBeginIndex = output.size();
					output.setSize(textBeginIndex + textSize + 3);
					char* textBegin = &(output[textBeginIndex]);
					textBegin[0] = '<';
					textBegin[1] = '/';
					textBegin += 2;
					for (size_t i = 0; i != textSize; ++i) {
						textBegin[i] = text[i];
					}
					textBegin += textSize;
					textBegin[0] = '>';
				}
				break;
			}
		}

		if (isSingleCase) {
			size_t textSize = item.text[0].size();
			output.setSize(textBeginIndex + beforeTextSize + textSize + afterTextSize);
			char* textBegin = &(output[textBeginIndex]);
			if (beforeTextSize != 0) {
				for (size_t i = 0; i != beforeTextSize; ++i) {
					textBegin[i] = beforeText[i];
				}
				textBegin += beforeTextSize;
			}
			for (size_t i = 0; i != textSize; ++i) {
				textBegin[i] = text[i];
			}
			textBegin += textSize;
			if (afterTextSize != 0) {
				for (size_t i = 0; i != afterTextSize; ++i) {
					textBegin[i] = afterText[i];
				}
				textBegin += afterTextSize;
			}
		}

		if (item.newLineAfter) {
			numTabs = nestingLevel;
			output.append('\n');
		}
	}
}

bool WriteXMLFile(const char* filename, const Content& content) {
	Array<char> contents;
	generateTextXML(content, contents, 0, 0);

	bool success = WriteWholeFile(filename, contents.data(), contents.size());
	return success;
}

} // namespace xml
OUTER_NAMESPACE_END
