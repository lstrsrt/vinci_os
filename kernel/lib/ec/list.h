#pragma once

#include "../base.h"

#define CONTAINING_RECORD(x, t, f) (( t* )(( char* )(x) - OFFSET(t, f)))

namespace ec
{
    struct slist_entry
    {
        static INLINE size_t size(slist_entry* head)
        {
            size_t size = 0;

            for (auto p = head; p && p->m_next != head; p = p->m_next)
                size++;

            return size;
        }

        INLINE void push(slist_entry* entry)
        {
            if (!m_next)
            {
                m_next = entry;
                m_next->m_next = m_next;
            }
            else
            {
                auto temp = m_next;
                while (temp->m_next != m_next)
                    temp = temp->m_next;
                entry->m_next = m_next;
                temp->m_next = entry;
                m_next = entry;
            }
        }

        INLINE auto pop()
        {
            if (!m_next)
                return;

            if (m_next->m_next == m_next)
            {
                m_next = nullptr;
            }
            else
            {
                auto temp = m_next;
                while (temp->m_next != m_next)
                    temp = temp->m_next;
                temp->m_next = m_next->m_next;
                m_next = m_next->m_next;
            }
        }

        INLINE void remove(slist_entry* entry)
        {
            if (!m_next)
                return;

            // The list is a ring, so find the entry before the one being removed.
            auto prev = m_next;
            while (prev->m_next != entry)
            {
                prev = prev->m_next;
                if (prev == m_next)
                    return; // Not in the list
            }

            if (prev == entry)
            {
                // It was the only entry
                m_next = nullptr;
                return;
            }

            prev->m_next = entry->m_next;
            if (m_next == entry)
                m_next = entry->m_next;
        }

        slist_entry* m_next{};
    };
}
