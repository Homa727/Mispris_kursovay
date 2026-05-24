#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlQuery>
#include <QSqlError>
#include <QVector>
#include <QMap>
#include <QDebug>
#include <QJsonArray>

struct Unit{
    int id;
    QString name;
    QString shortname;
};

struct ProductClass{
    int id;
    QString code;
    QString name;
    bool isTerminal;
    int baseUnitID;
    int parentID;
    int orderindex;
};

struct Product{
    int id;
    QString name;
    QString articleNumber;
    double price;
    QString manufacturer;
    int productclassID;
};

struct Enum{
    int id;
    QString name;
};

struct EnumValues{
    int id;
    int enumid;
    QString code;
    int orderIndex;
};

struct Parametr{
    int id;
    QString name;
    QString type;
    int enumID;
    int unitID;
    double minValue;
    double maxValue;
};

struct ProductparametrClass{
    int id;
    int ProductclassID;
    int parametrID;
};

struct ProductParametrValue{
    int id;
    int productID;
    int parameterID;
    double valueNumber;
    double EnumValueID;
};
struct OperationClass{
    int id;
    QString name;
    QString description;
};

struct OperationTemplate{
    int id;
    int classID;
    QString name;
    QString description;
};

struct Operation{
    int id;
    int templateID;
    QString operationDate;
    QString status;
};

struct OperationRole{
    int id;
    int operationID;
    QString roleName;
    QString participantName;
};

struct OperationDocument{
    int id;
    int operationID;
    QString documentType;
    QString documentNumber;
    QString documentDate;
};

struct OperationParameterValue{
    int id;
    int operationID;
    int parameterID;

    double numberValue;
    int enumValueID;

    QString stringValue;
};


class Database : public QObject
{
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    void connectToDatabase();
    void disconnectDatabase();
    void addUnit( const QString &name, const QString &shortmane);
    void deleteUnit(int id);
    bool setBaseUnit(int classID, int unitID);
    QVector<Unit> getAllUnits();

    bool AddProductClass(const ProductClass &cls);
    bool deleteProductClass(int id_for_del);
    bool moveProductClass(int classID, int newParentID);

    QVector<ProductClass> getAllProductClasses();
    QVector<ProductClass> getAllChild(int parentID);
    QVector<ProductClass> getAllParents(int classID);
    QVector<ProductClass> getTerminalClasses(int parentID);

    bool classCodeExists(const QString &code);
    bool checkCycle( int classID, int newParentID);

    bool addEnum(const QString &name);
    bool addEnumValue(const EnumValues &ptr);
    bool updateEnumValue(int id, QString newcode);
    bool deleteEnumValue(int id);
    bool changeEnumValueOrder(int id,int newOrderIndex);
    QVector<EnumValues> getEnumValues(int enumID);
    QVector<Enum> getEnums();
    QVector<EnumValues> getEnumValueByID(int id);

    bool changeOrder(int classID,int newOrderIndex);

    bool addParametr(const Parametr &param);
    bool addParametrToClass(int classID, int parametrID);
    QVector<Parametr> getClassParametr(int classID);

    bool setNumberParametrValue(int productID, int parametrID, double value);
    bool setEnumParametrValue(int productID, int parametrID, double enumValueID);
    QVector<ProductParametrValue> getProductParametrValue(int productID);

    // Поиск с учётом класса
    QVector<Product> findProductByNumberParam(int parametrID, double min, double max, int classId = 0);
    QVector<Product> findProductByEnumParam(int parametrID, int enumValueID, int classId = 0);
    QVector<Product> findProductByPrice(double min, double max, int classId = 0);

    // Многопараметрический поиск (поддерживает пустой массив условий)
    QVector<Product> multiSearch(int classId, const QJsonArray &conditions);

    bool addOperationClass(
        const OperationClass &cls);

    bool addOperationTemplate(
        const OperationTemplate &templ);

    bool createOperation(
        const Operation &op);

    bool addOperationRole(
        const OperationRole &role);

    bool addOperationDocument(
        const OperationDocument &doc);

    bool setOperationNumberParam(
        int operationID,
        int parameterID,
        double value);

    bool setOperationEnumParam(
        int operationID,
        int parameterID,
        int enumValueID);

    QVector<OperationClass> getOperationClasses();

    QVector<OperationTemplate> getOperationTemplates(
        int classID);

    QVector<Operation> getOperations();

    QVector<OperationRole> getOperationRoles(
        int operationID);

    QVector<OperationDocument> getOperationDocuments(
        int operationID);

    QVector<OperationParameterValue>
    getOperationParameters(
        int operationID);

private:
    QSqlDatabase database;
    ProductClass mapProductsClass(QSqlQuery &query);
    Product  mapProducts(QSqlQuery &query);
};

#endif // DATABASE_H
