#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <QWidget>

class Group;

class GroupWidget : public QWidget
{
    public:
        GroupWidget(Group* group, QWidget* parent);

        Id<Group> id() const;

    private:
        Group* m_group;
};
