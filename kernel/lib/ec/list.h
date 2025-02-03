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
            auto p1 = this;
            auto p2 = m_next;

            while (p2)
            {
                if (p2 == entry)
                {
                    p1->m_next = p2->m_next;
                    return;
                }

                p1 = p1->m_next;
                p2 = p2->m_next;
            }
        }

        slist_entry* m_next{};
    };
}
