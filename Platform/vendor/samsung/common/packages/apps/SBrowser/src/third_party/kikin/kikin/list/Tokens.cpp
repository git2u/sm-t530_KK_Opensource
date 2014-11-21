/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#include "config.h"
#include "kikin/list/Tokens.h"

#include "kikin/html/HtmlToken.h"

namespace kikin { namespace list {

Tokens::Tokens()
{
}

Tokens::~Tokens()
{
}
    
void Tokens::append(PassRefPtr<HtmlToken> token)
{
    if (token)
        m_list.append(token);
}
    
unsigned Tokens::length() const
{
    return m_list.size();
}

HtmlToken* Tokens::item(unsigned index)
{
    if (index >= m_list.size()) {
        // FIXME: this should throw an exception.
        // ec = INDEX_SIZE_ERR;
        return 0;
    }

    return m_list[index].get();
}

HtmlToken* Tokens::findSelected()
{
	for (size_t i = 0; i < m_list.size(); i++) {
		HtmlToken* token = m_list[i].get();
		if (token->isSelected()) {
			return token;
		}
	}
	return 0;
}
    
} // namespace list
    
} // namespace kikin
