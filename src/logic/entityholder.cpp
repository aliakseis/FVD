#include "entityholder.h"
#include "remotevideoentity.h"

EntityHolder::EntityHolder() : m_currentEntity(nullptr)
{
}

void EntityHolder::setEntity(RemoteVideoEntity* data)
{
	m_currentEntity = data;
}

void EntityHolder::resetEntity()
{
	m_currentEntity = nullptr;
}
