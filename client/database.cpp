#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>
#include "database.h"

Database::Database(QObject *parent) : QObject(parent) {}
Database::~Database() {}

void Database::connectToDatabase() {
    database = QSqlDatabase::addDatabase("QODBC");
    QString connectString = "DRIVER={ODBC Driver 17 for SQL Server};";
    connectString.append("SERVER=localhost;");
    connectString.append("Trusted_Connection=yes;");
    connectString.append("DATABASE=mispris;");
    database.setDatabaseName(connectString);
    if (database.open()) {
        qDebug() << "База данных успешно подключена";
    } else {
        QSqlError err = database.lastError();
        qDebug() << "Ошибка подключения к базе данных"<<err.text();
    }
}

void Database::disconnectDatabase() {
    if (database.isOpen()) {
        database.close();
        qDebug() << "База данных отключена";
    }
}

bool Database::AddProductClass(const ProductClass &cls){
    if (classCodeExists(cls.code)) {
        qDebug() << "Ошибка добавления: код" << cls.code << "уже существует";
            return false;
    }
    if (cls.parentID != 0) {
        QSqlQuery checkParent;
        checkParent.prepare("SELECT isTerminal FROM ProductClass WHERE id = :parentID");
        checkParent.bindValue(":parentID", cls.parentID);
        if (!checkParent.exec() || !checkParent.next()) {
            qDebug() << "Ошибка добавления: родительский класс с id" << cls.parentID << "не найден";
            return false;
        }
        bool isParentTerminal = checkParent.value(0).toBool();
        if (isParentTerminal) {
            qDebug() << "Ошибка добавления: родительский класс является терминальным, нельзя добавлять потомков";
            return false;
        }
    }
    QSqlQuery query;
    query.prepare("INSERT INTO ProductClass(code, imay, isTerminal, baseUnitID, parentID, orderIndex) "
                  "VALUES(:code, :name, :isTerminal, :baseUnitID, :parentID, :orderIndex)");
    query.bindValue(":code", cls.code);
    query.bindValue(":name", cls.name);
    query.bindValue(":isTerminal", cls.isTerminal);
    query.bindValue(":baseUnitID", cls.baseUnitID);
    query.bindValue(":parentID", cls.parentID);
    query.bindValue(":orderIndex", cls.orderindex);
    return query.exec();
}

void Database::addUnit(const QString &name, const QString &shortmane){
    QSqlQuery query;
    query.prepare("INSERT INTO Units(imya, shortName) VALUES(:name, :shortName)");
    query.bindValue(":name" , name);
    query.bindValue(":shortName" , shortmane);
    query.exec();
}

bool Database::deleteProductClass(int id_for_del){
    QSqlQuery checkChildren;
    checkChildren.prepare("SELECT COUNT(*) FROM ProductClass WHERE parentID = :id");
    checkChildren.bindValue(":id", id_for_del);
    if (!checkChildren.exec() || !checkChildren.next()) {
        qDebug() << "Ошибка при проверке потомков:" << checkChildren.lastError().text();
            return false;
    }
    int childCount = checkChildren.value(0).toInt();
    if (childCount > 0) {
        qDebug() << "Ошибка удаления: у класса есть" << childCount << "потомков. Сначала удалите их.";
        return false;
    }
    QSqlQuery query;
    query.prepare("DELETE FROM ProductClass WHERE id=:id");
    query.bindValue(":id", id_for_del);
    return query.exec();
}

void Database::deleteUnit(int id){
    QSqlQuery query;
    query.prepare("DELETE FROM Units WHERE id=:id");
    query.bindValue(":id", id);
    query.exec();
}

bool Database::moveProductClass(int classID, int newParentID){
    QSqlQuery query;
    query.prepare("UPDATE ProductClass SET parentID = :newParentID WHERE id=:classID");
    query.bindValue(":classID", classID);
    query.bindValue(":newParentID", newParentID);
    return query.exec();
}

QVector<ProductClass> Database::getAllParents(int classID)
{
    QVector<ProductClass> result;
    QSqlQuery query;
    query.prepare(
        "WITH ParentTree AS ( "
        "SELECT * FROM ProductClass WHERE id = :id "
        "UNION ALL "
        "SELECT pc.* "
        "FROM ProductClass pc "
        "JOIN ParentTree pt ON pt.parentID = pc.id ) "
        "SELECT * FROM ParentTree WHERE id <> :id");
    query.bindValue(":id", classID);
    query.exec();
    while(query.next())
    {
        ProductClass pc;
        pc.id = query.value("id").toInt();
        pc.name = query.value("imay").toString();
        result.push_back(pc);
    }
    return result;
}

QVector<ProductClass> Database::getTerminalClasses(int parentID){
    QVector<ProductClass> result;
    QSqlQuery query;
    query.prepare(
        "WITH ClassTree AS ( "
        "SELECT * FROM ProductClass WHERE id = :id "
        "UNION ALL "
        "SELECT pc.* "
        "FROM ProductClass pc "
        "JOIN ClassTree ct ON pc.parentID = ct.id ) "
        "SELECT * FROM ClassTree WHERE isTerminal = 1");
    query.bindValue(":id", parentID);
    query.exec();
    while(query.next())
    {
        ProductClass pc;
        pc.id = query.value("id").toInt();
        pc.name = query.value("imay").toString();
        result.push_back(pc);
    }
    return result;
}

bool Database::checkCycle(int classID, int parentID)
{
    QSqlQuery query;
    query.prepare(
        "WITH Tree AS ( "
        "SELECT id,parentID FROM ProductClass WHERE id = :parent "
        "UNION ALL "
        "SELECT pc.id, pc.parentID "
        "FROM ProductClass pc "
        "JOIN Tree t ON pc.id = t.parentID ) "
        "SELECT id FROM Tree WHERE id = :class");
    query.bindValue(":parent", parentID);
    query.bindValue(":class", classID);
    query.exec();
    return query.next();
}

QVector<ProductClass> Database::getAllChild(int parentID)
{
    QVector<ProductClass> result;
    QSqlQuery query;
    query.prepare(
        "WITH ClassTree AS ( "
        "SELECT * FROM ProductClass WHERE id = :id "
        "UNION ALL "
        "SELECT pc.* "
        "FROM ProductClass pc "
        "JOIN ClassTree ct ON pc.parentID = ct.id ) "
        "SELECT * FROM ClassTree WHERE id <> :id");
    query.bindValue(":id", parentID);
    query.exec();
    while(query.next())
    {
        ProductClass pc;
        pc.id = query.value("id").toInt();
        pc.code = query.value("code").toString();
        pc.name = query.value("imay").toString();
        result.push_back(pc);
    }
    return result;
}

bool Database::setBaseUnit(int classID, int unitID)
{
    QSqlQuery query;
    query.prepare("UPDATE ProductClass SET baseUnitID = :unit WHERE id = :id");
    query.bindValue(":unit", unitID);
    query.bindValue(":id", classID);
    return query.exec();
}

bool Database::changeOrder(int classID, int newOrder)
{
    QSqlQuery query;
    query.prepare("UPDATE ProductClass SET orderIndex = :order WHERE id = :id");
    query.bindValue(":order", newOrder);
    query.bindValue(":id", classID);
    return query.exec();
}

QVector<ProductClass> Database::getAllProductClasses()
{
    QVector<ProductClass> result;
    QSqlQuery query;
    query.prepare("SELECT * FROM ProductClass ORDER BY orderIndex");
    if(!query.exec())
    {
        qDebug() << query.lastError();
        return result;
    }
    while(query.next())
    {
        result.push_back(mapProductsClass(query));
    }
    return result;
}

bool Database::classCodeExists(const QString &code)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM ProductClass WHERE code = :code");
    query.bindValue(":code", code);
    if(!query.exec())
    {
        qDebug() << query.lastError();
    }
    return query.next();
}

QVector<Unit> Database::getAllUnits()
{
    QVector<Unit> result;
    QSqlQuery query("SELECT * FROM Units");
    while(query.next())
    {
        Unit u;
        u.id = query.value("id").toInt();
        u.name = query.value("imya").toString();
        u.shortname = query.value("shortName").toString();
        result.push_back(u);
    }
    return result;
}

ProductClass Database::mapProductsClass(QSqlQuery &query)
{
    ProductClass pc;
    pc.id = query.value("id").toInt();
    pc.code = query.value("code").toString();
    pc.name = query.value("imay").toString();
    pc.isTerminal = query.value("isTerminal").toBool();
    pc.baseUnitID = query.value("baseUnitID").toInt();
    pc.parentID = query.value("parentID").toInt();
    pc.orderindex = query.value("orderIndex").toInt();
    return pc;
}

bool Database::addEnum(const QString &name){
    QSqlQuery query;
    query.prepare("INSERT INTO [Enum] (imya) VALUES(:name)");
    query.bindValue(":name",name);
    return query.exec();
}

bool Database::addEnumValue(const EnumValues &ptr){
    QSqlQuery query;
    query.prepare("INSERT INTO [EnumValues] (enumID, imya, orderID) VALUES(:enumID, :code, :orderID)");
    query.bindValue(":enumID", ptr.enumid);
    query.bindValue(":code",ptr.code);
    query.bindValue(":orderID",ptr.orderIndex);
    return query.exec();
}

bool Database::changeEnumValueOrder(int id, int newOrderIndex){
    QSqlQuery query;
    query.prepare("UPDATE [EnumValues] SET orderID=:newOrderIndex WHERE id=:id");
    query.bindValue(":newOrderIndex",newOrderIndex);
    query.bindValue(":id",id);
    return query.exec();
}

bool Database::deleteEnumValue(int id){
    QSqlQuery query;
    query.prepare("DELETE FROM [EnumValues] WHERE id=:id");
    query.bindValue(":id",id);
    return query.exec();
}

bool Database::updateEnumValue(int id, QString newcode){
    QSqlQuery query;
    query.prepare("UPDATE [EnumValues] SET imya=:newcode WHERE id=:id");
    query.bindValue(":newcode",newcode);
    query.bindValue(":id",id);
    return query.exec();
}

QVector<Enum> Database::getEnums(){
    QVector<Enum> result;
    QSqlQuery query;
    query.prepare("SELECT * FROM dbo.[Enum]");
    query.exec();
    while(query.next()){
        Enum e;
        e.id=query.value("id").toInt();
        e.name=query.value("imya").toString();
        qDebug() << "Fetched:" << e.id << e.name;
        result.push_back(e);
    }
    qDebug() << query.lastError();
    return result;
}

QVector<EnumValues> Database::getEnumValues(int enumID){
    QVector<EnumValues> result;
    QSqlQuery query;
    query.prepare("SELECT * FROM [EnumValues] WHERE enumID=:enumID");
    query.bindValue(":enumID",enumID);
    query.exec();
    while(query.next()){
        EnumValues v;
        v.id=query.value("id").toInt();
        v.enumid=query.value("enumID").toInt();
        v.code=query.value("imya").toString();
        v.orderIndex=query.value("orderID").toInt();
        result.push_back(v);
    }
    return result;
}

QVector<EnumValues> Database::getEnumValueByID(int id){
    QVector<EnumValues> result;
    QSqlQuery query;
    query.prepare("SELECT * FROM [EnumValues] WHERE id=:id");
    query.bindValue(":id",id);
    query.exec();
    while(query.next()){
        EnumValues val;
        val.id=query.value("id").toInt();
        val.enumid=query.value("enumID").toInt();
        val.code=query.value("imya").toString();
        val.orderIndex=query.value("orderID").toInt();
        result.push_back(val);
    }
    qDebug() << query.lastError();
    return result;
}

bool Database::addParametr(const Parametr &param){
    QSqlQuery query;
    query.prepare("INSERT INTO Parametr (name,type,enumID,unitID,minValue,maxValue) VALUES(:name, :type, :enumID, :unitID, :minValue, :maxValue)");
    query.bindValue(":name",param.name);
    query.bindValue(":type",param.type);
    query.bindValue(":enumID",param.enumID);
    query.bindValue(":unitID",param.unitID);
    query.bindValue(":minValue",param.minValue);
    query.bindValue(":maxValue",param.maxValue);
    return query.exec();
}

bool Database::addParametrToClass(int classID, int parametrID){
    QSqlQuery query;
    query.prepare("INSERT INTO ProductclassParametr (ProductclassID,parametrID) VALUES(:classID, :parametrID)");
    query.bindValue(":classID",classID);
    query.bindValue(":parametrID",parametrID);
    return query.exec();
}

QVector<Parametr> Database::getClassParametr(int classID){
    QVector<Parametr> result;
    QSqlQuery query;
    query.prepare("SELECT p.* FROM parametr p JOIN ProductclassParametr pc ON p.id = pc.parametrID WHERE pc.ProductclassID = :classID ");
    query.bindValue(":classID",classID);
    query.exec();
    while(query.next()){
        Parametr par;
        par.id=query.value("id").toInt();
        par.name=query.value("name").toString();
        par.type=query.value("type").toString();
        par.enumID=query.value("enumID").toInt();
        par.unitID=query.value("unitID").toInt();
        par.minValue=query.value("minValue").toInt();
        par.maxValue=query.value("maxValue").toInt();
        result.push_back(par);
    }
    return result;
}

bool Database::setNumberParametrValue(int productID, int parametrID, double value){
    QSqlQuery query;
    query.prepare("UPDATE ProductParameterValue SET valueNumber=:value WHERE productID=:productID AND parameterID=:parametrID");
    query.bindValue(":productID",productID);
    query.bindValue(":parametrID",parametrID);
    query.bindValue(":value",value);
    return query.exec();
}

bool Database::setEnumParametrValue(int productID, int parametrID, double enumValueID){
    QSqlQuery query;
    query.prepare("UPDATE ProductParameterValue SET valueEnumID=:enumValueID WHERE productID=:productID AND parameterID=:parametrID");
    query.bindValue(":productID",productID);
    query.bindValue(":parametrID",parametrID);
    query.bindValue(":enumValueID",enumValueID);
    return query.exec();
}

QVector<ProductParametrValue> Database::getProductParametrValue(int productID){
    QVector<ProductParametrValue> result;
    QSqlQuery query;
    query.prepare("SELECT* FROM ProductParameterValue WHERE productID=:productID");
    query.bindValue(":productID",productID);
    query.exec();
    while(query.next()){
        ProductParametrValue pro;
        pro.id=query.value("id").toInt();
        pro.productID=query.value("productID").toInt();
        pro.parameterID=query.value("parameterID").toInt();
        pro.valueNumber=query.value("valueNumber").toDouble();
        pro.EnumValueID=query.value("valueEnumID").toInt();
        result.push_back(pro);
    }
    return result;
}

QVector<Product> Database::findProductByNumberParam(int parameterID, double min, double max, int classId)
{
    QVector<Product> result;
    QSqlQuery query;
    QString sql = "SELECT t.* FROM tovar t "
                  "JOIN ProductParameterValue ppv ON t.id = ppv.productID "
                  "WHERE ppv.parameterID = :paramID "
                  "AND ppv.valueNumber BETWEEN :min AND :max";
    if (classId > 0) sql += " AND t.productClassID = :classId";
    query.prepare(sql);
    query.bindValue(":paramID", parameterID);
    query.bindValue(":min", min);
    query.bindValue(":max", max);
    if (classId > 0) query.bindValue(":classId", classId);
    if (!query.exec()) {
        qDebug() << query.lastError();
        return result;
    }
    while(query.next()){
        Product p;
        p.id = query.value("id").toInt();
        p.name = query.value("imya").toString();
        p.articleNumber = query.value("articleNumber").toString();
        p.price = query.value("price").toDouble();
        p.manufacturer = query.value("manufacture").toString();
        p.productclassID = query.value("productClassID").toInt();
        result.push_back(p);
    }
    return result;
}

QVector<Product> Database::findProductByEnumParam(int parameterID, int enumValueID, int classId)
{
    QVector<Product> result;
    QSqlQuery query;
    QString sql = "SELECT t.* FROM tovar t "
                  "JOIN ProductParameterValue ppv ON t.id = ppv.productID "
                  "WHERE ppv.parameterID = :paramID "
                  "AND ppv.valueEnumID = :enumID";
    if (classId > 0) sql += " AND t.productClassID = :classId";
    query.prepare(sql);
    query.bindValue(":paramID", parameterID);
    query.bindValue(":enumID", enumValueID);
    if (classId > 0) query.bindValue(":classId", classId);
    if (!query.exec()) {
        qDebug() << query.lastError();
        return result;
    }
    while(query.next()){
        Product p;
        p.id = query.value("id").toInt();
        p.name = query.value("imya").toString();
        p.articleNumber = query.value("articleNumber").toString();
        p.price = query.value("price").toDouble();
        p.manufacturer = query.value("manufacture").toString();
        p.productclassID = query.value("productClassID").toInt();
        result.push_back(p);
    }
    return result;
}

QVector<Product> Database::findProductByPrice(double min, double max, int classId)
{
    QVector<Product> result;
    QSqlQuery query;
    QString sql = "SELECT * FROM tovar WHERE price BETWEEN :min AND :max";
    if (classId > 0) sql += " AND productClassID = :classId";
    query.prepare(sql);
    query.bindValue(":min", min);
    query.bindValue(":max", max);
    if (classId > 0) query.bindValue(":classId", classId);
    if (!query.exec()) {
        qDebug() << query.lastError();
        return result;
    }
    while(query.next()){
        Product p;
        p.id = query.value("id").toInt();
        p.name = query.value("imya").toString();
        p.articleNumber = query.value("articleNumber").toString();
        p.price = query.value("price").toDouble();
        p.manufacturer = query.value("manufacture").toString();
        p.productclassID = query.value("productClassID").toInt();
        result.push_back(p);
    }
    return result;
}

QVector<Product> Database::multiSearch(int classId, const QJsonArray &conditions)
{
    QVector<Product> result;

    // Если массив условий пуст – возвращаем все товары выбранного класса
    if (conditions.isEmpty()) {
        QSqlQuery query;
        query.prepare("SELECT * FROM tovar WHERE productClassID = :classId");
        query.bindValue(":classId", classId);
        if (!query.exec()) {
            qDebug() << query.lastError();
            return result;
        }
        while (query.next()) {
            Product p;
            p.id = query.value("id").toInt();
            p.name = query.value("imya").toString();
            p.articleNumber = query.value("articleNumber").toString();
            p.price = query.value("price").toDouble();
            p.manufacturer = query.value("manufacture").toString();
            p.productclassID = query.value("productClassID").toInt();
            result.push_back(p);
        }
        return result;
    }

    // Иначе строим сложный запрос с JOIN и WHERE
    QString sql = "SELECT DISTINCT t.* FROM tovar t ";
    QStringList joins;
    QStringList whereClauses;
    int joinCounter = 0;

    whereClauses << "t.productClassID = :classId";

    for (const auto &condVal : conditions) {
        QJsonObject cond = condVal.toObject();
        QString type = cond["type"].toString();
        if (type == "price") {
            double minPrice = cond["minPrice"].toDouble();
            double maxPrice = cond["maxPrice"].toDouble();
            whereClauses << QString("t.price BETWEEN :minPrice_%1 AND :maxPrice_%1").arg(joinCounter);
        }
        else if (type == "number") {
            int paramId = cond["paramId"].toInt();
            double minVal = cond["minValue"].toDouble();
            double maxVal = cond["maxValue"].toDouble();
            QString alias = QString("ppv_%1").arg(joinCounter);
            joins << QString("JOIN ProductParameterValue %1 ON t.id = %1.productID AND %1.parameterID = :paramId_%2")
                         .arg(alias).arg(joinCounter);
            whereClauses << QString("%1.valueNumber BETWEEN :minVal_%2 AND :maxVal_%2")
                                .arg(alias).arg(joinCounter);
        }
        else if (type == "enum") {
            int paramId = cond["paramId"].toInt();
            int enumValId = cond["enumValueId"].toInt();
            QString alias = QString("ppv_%1").arg(joinCounter);
            joins << QString("JOIN ProductParameterValue %1 ON t.id = %1.productID AND %1.parameterID = :paramId_%2")
                         .arg(alias).arg(joinCounter);
            whereClauses << QString("%1.valueEnumID = :enumValId_%2")
                                .arg(alias).arg(joinCounter);
        }
        joinCounter++;
    }

    sql += joins.join(" ");
    sql += " WHERE " + whereClauses.join(" AND ");

    QSqlQuery query;
    query.prepare(sql);
    query.bindValue(":classId", classId);

    int bindCounter = 0;
    for (const auto &condVal : conditions) {
        QJsonObject cond = condVal.toObject();
        QString type = cond["type"].toString();
        if (type == "price") {
            query.bindValue(QString(":minPrice_%1").arg(bindCounter), cond["minPrice"].toDouble());
            query.bindValue(QString(":maxPrice_%1").arg(bindCounter), cond["maxPrice"].toDouble());
        }
        else if (type == "number") {
            query.bindValue(QString(":paramId_%1").arg(bindCounter), cond["paramId"].toInt());
            query.bindValue(QString(":minVal_%1").arg(bindCounter), cond["minValue"].toDouble());
            query.bindValue(QString(":maxVal_%1").arg(bindCounter), cond["maxValue"].toDouble());
        }
        else if (type == "enum") {
            query.bindValue(QString(":paramId_%1").arg(bindCounter), cond["paramId"].toInt());
            query.bindValue(QString(":enumValId_%1").arg(bindCounter), cond["enumValueId"].toInt());
        }
        bindCounter++;
    }

    if (!query.exec()) {
        qDebug() << "MultiSearch error:" << query.lastError();
        return result;
    }

    while (query.next()) {
        Product p;
        p.id = query.value("id").toInt();
        p.name = query.value("imya").toString();
        p.articleNumber = query.value("articleNumber").toString();
        p.price = query.value("price").toDouble();
        p.manufacturer = query.value("manufacture").toString();
        p.productclassID = query.value("productClassID").toInt();
        result.push_back(p);
    }
    return result;
}
bool Database::addOperationClass(const OperationClass &cls){
    QSqlQuery query;
    query.prepare("INSERT INTO OperationClass(name,description) VALUES(:name,:description)");
    query.bindValue(":name", cls.name);
    query.bindValue(":name", cls.description);
    return query.exec();

}
bool Database::addOperationDocument(const OperationDocument &doc){
    QSqlQuery query;
    query.prepare("INSERT INTO OperationDocument(operationID,documentType,documentNumber,documentDate) VALUES(:operationID,:documentType,:documentNumber,:documentDate)");
    query.bindValue(":operationID", doc.operationID);
    query.bindValue(":documentType", doc.documentType);
    query.bindValue(":documentNumber", doc.documentNumber);
    query.bindValue(":documentDate", doc.documentDate);
    return query.exec();

}
bool Database::addOperationRole(const OperationRole &role){
    QSqlQuery query;
    query.prepare("INSERT INTO OperationRole(operationID,roleName,participantName) VALUES(:operationID,:roleName,:participantName)");
    query.bindValue(":operationID", role.operationID);
    query.bindValue(":documentNumber", role.roleName);
    query.bindValue(":documentType", role.participantName);
    return query.exec();
}
bool Database::setOperationNumberParam(int operationID,int parameterID,double value){
    QSqlQuery query;
    query.prepare("UPDATE OperationParameterValue SET numberValue=:value WHERE operationID=:operationID AND parameterID=:parameterID");
    query.bindValue(":value",value);
    query.bindValue(":operationID",operationID);
    query.bindValue(":parameterID",parameterID);
    return query.exec();
}
bool Database::addOperationTemplate(const OperationTemplate &templ){
    QSqlQuery query;
    query.prepare("INSERT INTO OperationTemplate(classID,name,description) VALUES(:classID,:name,:description)");
    query.bindValue(":classID", templ.classID);
    query.bindValue(":name", templ.name);
    query.bindValue(":description", templ.description);
    return query.exec();
}
bool Database::createOperation(const Operation &op){
    QSqlQuery query;
    query.prepare("INSERT INTO Operation(templateID,status) VALUES(:templateID,:status)");
    query.bindValue(":templateID", op.templateID);
    //query.bindValue(":operationDate", op.operationDate);
    query.bindValue(":status", op.status);
    return query.exec();
}
bool Database::setOperationEnumParam(int operationID, int parameterID, int enumValueID){
    QSqlQuery query;
    query.prepare("UPDATE OperationParameterValue SET enumValueID=:enumValueID WHERE operationID=:operationID AND parameterID=:parameterID");
    query.bindValue(":enumValueID",enumValueID);
    query.bindValue(":operationID",operationID);
    query.bindValue(":parameterID",parameterID);
    return query.exec();
}
QVector<OperationClass>Database::getOperationClasses(){
    QVector<OperationClass> result;
    QSqlQuery query;
    query.prepare("SELECT * FROM OperationClass");
    query.exec();
    while(query.next()){
        OperationClass op;
        op.id=query.value("id").toInt();
        op.name=query.value("name").toString();
        op.description=query.value("description").toString();
        result.push_back(op);
    }
    return result;
}
QVector<OperationTemplate>Database::getOperationTemplates(int classID){
    QVector<OperationTemplate> result;
    QSqlQuery query;
    query.prepare("SELECT * FROM OperationTemplate WHERE classID=:classID");
    query.bindValue(":classID",classID);
    query.exec();
    while(query.next()){
        OperationTemplate op;
        op.id=query.value("id").toInt();
        op.classID=query.value("classID").toInt();
        op.name=query.value("name").toString();
        op.description=query.value("description").toString();
        result.push_back(op);
    }
    return result;
}
QVector<Operation>Database::getOperations(){
    QVector<Operation> result;
    QSqlQuery query;
    query.prepare("SELECT * FROM Operation");
    query.exec();
    while(query.next()){
        Operation op;
        op.id=query.value("id").toInt();
        op.templateID=query.value("templateID").toInt();
        op.operationDate=query.value("operationDate").toString();
        op.status=query.value("status").toString();
        result.push_back(op);
    }
    return result;
}
QVector<OperationRole>Database::getOperationRoles(int operationID){
    QVector<OperationRole> result;
    QSqlQuery query;
    query.prepare("SELECT* FROM OperationRole WHERE operationID=:operationID");
    query.bindValue(":operationID",operationID);
    query.exec();
    while(query.next()){
        OperationRole op;
        op.id=query.value("id").toInt();
        op.operationID=query.value("operationID").toInt();
        op.participantName=query.value("participantName").toString();
        op.roleName=query.value("roleName").toString();
        result.push_back(op);
    }
    return result;
}
QVector<OperationDocument>Database::getOperationDocuments(int operationID){
    QVector<OperationDocument> result;
    QSqlQuery query;
    query.prepare("SELECT * FROM OperationDocument WHERE operationID=:operationID");
    query.bindValue(":operationID",operationID);
    query.exec();
    while(query.next()){
       OperationDocument op;
       op.id=query.value("id").toInt();
       op.operationID=query.value("operationID").toInt();
       op.documentDate=query.value("documentDate").toString();
       op.documentNumber=query.value("documentNumber").toString();
       op.documentType=query.value("documentType").toString();
       result.push_back(op);
    }
    return result;
}
QVector<OperationParameterValue>Database::getOperationParameters(int operationID){
    QVector<OperationParameterValue> result;
    QSqlQuery query;
    query.prepare("SELECT * FROM OperationParameterValue WHERE operationID=:operationID");
    query.bindValue(":operationID",operationID);
    query.exec();
    while(query.next()){
        OperationParameterValue op;
        op.id=query.value("id").toInt();
        op.operationID=query.value("operationID").toInt();
        op.parameterID=query.value("parameterID").toInt();
        op.stringValue=query.value("stringValue").toString();
        op.enumValueID=query.value("enumValueID").toInt();
        op.numberValue=query.value("numberValue").toDouble();
        result.push_back(op);
    }
    return result;
}
