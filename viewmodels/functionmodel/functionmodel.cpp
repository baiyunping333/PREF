#include "functionmodel.h"

FunctionModel::FunctionModel(DisassemblerListing* listing, QObject *parent): QAbstractItemModel(parent), _listing(listing)
{
    this->_monospacefont.setFamily("Monospace");
    this->_monospacefont.setPointSize(qApp->font().pointSize());
    this->_monospacefont.setStyleHint(QFont::TypeWriter);

    this->_functions = listing->functions().values();
}

QString FunctionModel::functionType(Function *f) const
{
    switch(f->type())
    {
        case FunctionType::EntryPointFunction:
            return "Entry Point";

        case FunctionType::ImportFunction:
            return "Import";

        case FunctionType::ExportFunction:
            return "Export";

        default:
            break;
    }

    return "Function";
}

int FunctionModel::columnCount(const QModelIndex &) const
{
    return 4;
}

QVariant FunctionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch(section)
        {
            case 0:
                return "Address";

            case 1:
                return "Segment";

            case 2:
                return "Type";

            case 3:
                return "Name";

            default:
                break;
        }
    }

    return QVariant();
}

QVariant FunctionModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    Function* f = reinterpret_cast<Function*>(index.internalPointer());

    if(role == Qt::DisplayRole)
    {   
        switch(index.column())
        {
            case 0:
                return f->startAddress().toString(16);

            case 1:
                return this->_listing->findSegment(f)->name();

            case 2:
                return this->functionType(f);

            case 3:
                return this->_listing->symbolTable()->name(f->startAddress());

            default:
                break;
        }
    }
    else if(role == Qt::BackgroundRole)
    {
        switch(f->type())
        {
            case FunctionType::EntryPointFunction:
                return QColor(Qt::yellow);

            case FunctionType::ImportFunction:
                return QColor(Qt::cyan);

            case FunctionType::ExportFunction:
                return QColor(Qt::darkYellow);

            default:
                break;
        }
    }
    else if(role == Qt::ForegroundRole)
    {
        if(index.column() == 0)
            return QColor(Qt::darkBlue);

        if(index.column() == 1)
            return QColor(Qt::darkMagenta);

        if(index.column() == 2)
            return QColor(Qt::darkRed);
    }
    else if(role == Qt::FontRole)
        return this->_monospacefont;

    return QVariant();
}

QModelIndex FunctionModel::index(int row, int column, const QModelIndex &parent) const
{
    if(!this->hasIndex(row, column, parent))
        return QModelIndex();

    return this->createIndex(row, column, this->_functions[row]);
}

QModelIndex FunctionModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

int FunctionModel::rowCount(const QModelIndex &) const
{
    return this->_functions.count();
}

Qt::ItemFlags FunctionModel::flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}
