#include "CommentBlockModel.hpp"

#include <QTextDocument>

CommentBlockModel::CommentBlockModel(const Id<CommentBlockModel>& id,
                           const TimeValue& date,
                           double yPos,
                           QObject* parent):
    IdentifiedObject<CommentBlockModel>{id, "CommentBlockModel", parent},
    m_date{date},
    m_yposition{yPos}
{
    m_content = new QTextDocument{"Hello", this};
}

void CommentBlockModel::setDate(const TimeValue& date)
{
    if(date != m_date)
    {
        m_date = date;
        emit dateChanged(m_date);
    }
}

const TimeValue&CommentBlockModel::date() const
{
    return m_date;
}

double CommentBlockModel::heightPercentage() const
{
    return m_yposition;
}

void CommentBlockModel::setHeightPercentage(double y)
{
    m_yposition = y;
}
