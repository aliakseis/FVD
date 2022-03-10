#pragma once

class RemoteVideoEntity;

class EntityHolder
{
public:
	EntityHolder();
	virtual ~EntityHolder() {};

	void setEntity(RemoteVideoEntity* data);
	const RemoteVideoEntity* entity() const { return m_currentEntity; }
	void resetEntity();

protected:
	RemoteVideoEntity* m_currentEntity;
};
