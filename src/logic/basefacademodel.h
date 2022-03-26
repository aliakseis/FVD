#pragma once

#include <QHash>
#include <QList>
#include <algorithm>
#include <unordered_set>

#include "qglobal.h"

template <class StorageType>
class BaseFacadeModel
{
public:
    BaseFacadeModel()
    {
        m_entities.reserve(128);
        entInd.reserve(128);
    }

    // returns model row-index of the entity
    int entityRow(const StorageType* const entity) const
    {
        auto foundIt = entInd.find(entity);
        return (foundIt != entInd.constEnd() ? foundIt.value() : -1);
    }

    // returns the entity by row-index
    StorageType* item(int index) const
    {
        return (index >= 0 && index < m_entities.size() ? m_entities.at(index) : nullptr);
    }

    template <class Fn>
    void iterateEntities(Fn fn) const
    {
        std::for_each(m_entities.constBegin(), m_entities.constEnd(), fn);
    }

    void addEntities_impl(const QList<StorageType*>& added)
    {
        int startInd = m_entities.size();
        m_entities.append(added);
        updateInds(startInd);
    }

    void prependEntities_impl(const QList<StorageType*>& added)
    {
        for (int i = added.size() - 1; i >= 0; --i)
        {
            m_entities.prepend(added.at(i));
        }
        updateInds();
    }

    void reorder(const QList<StorageType*>& newList)  // TODO:  function must take reordering data and consider filter
    {
        Q_ASSERT(newList.size() == m_entities.size());
        m_entities.clear();
        m_entities = newList;
        updateInds();
    }

protected:
    void removeEntities_impl(const QList<StorageType*>& removed)
    {
        std::unordered_set<StorageType*> removedSet(removed.begin(), removed.end());

        auto it = std::remove_if(m_entities.begin(), m_entities.end(),
                                 [&removedSet](StorageType* ent) { return removedSet.find(ent) != removedSet.end(); });

        m_entities.erase(it, m_entities.end());

        updateInds();
    }

    int numEntities() const { return m_entities.size(); }

    void clear_impl()
    {
        m_entities.clear();
        entInd.clear();
    }

    QList<StorageType*> removeEntities_impl(int beg, int count)
    {
        QList<StorageType*> result;
        if (beg >= 0 && count > 0 && (beg + count <= m_entities.size()))
        {
            auto beg_it = m_entities.begin() + beg;
            auto end_it = beg_it + count;
            std::copy(beg_it, end_it, std::back_inserter(result));
            m_entities.erase(beg_it, end_it);

            updateInds();
        }
        else
        {
            Q_ASSERT(false && "Incorrect parameters");
        }

        return result;
    }
    template <class Pred>
    StorageType* findEntity(Pred& pred) const
    {
        auto it = std::find_if(m_entities.begin(), m_entities.end(),
                               [&pred](const StorageType* const ent) { return pred(ent); });
        return (it != m_entities.end() ? *it : nullptr);
    }

private:
    QList<StorageType*> m_entities;
    QHash<const StorageType*, int> entInd;

    void updateInds(int counter = 0)
    {
        if (entInd.size() != counter)
        {
            entInd.clear();
            counter = 0;
        }
        for (auto item = m_entities.constBegin() + counter; item != m_entities.constEnd(); ++item, ++counter)
        {
            entInd.insert(*item, counter);
        }
    }
};
