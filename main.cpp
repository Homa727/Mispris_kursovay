#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QUrlQuery>
#include <QMap>
#include <QFile>
#include <QMimeDatabase>
#include "database.h"

Database *g_db = nullptr;

QJsonObject productClassToJson(const ProductClass &pc) {
    QJsonObject obj;
    obj["id"] = pc.id;
    obj["code"] = pc.code;
    obj["name"] = pc.name;
    obj["isTerminal"] = pc.isTerminal;
    obj["baseUnitID"] = pc.baseUnitID;
    obj["parentID"] = pc.parentID;
    obj["orderIndex"] = pc.orderindex;
    return obj;
}

QJsonObject unitToJson(const Unit &u) {
    QJsonObject obj;
    obj["id"] = u.id;
    obj["name"] = u.name;
    obj["shortName"] = u.shortname;
    return obj;
}

QJsonObject EnumToJson(const Enum &E){
    QJsonObject obj;
    obj["id"]=E.id;
    obj["name"]=E.name;
    return obj;
}

QJsonObject EnumValuesToJson( const EnumValues &val){
    QJsonObject obj;
    obj["id"]=val.id;
    obj["enumid"]=val.enumid;
    obj["code"]=val.code;
    obj["orderIndex"]=val.orderIndex;
    return obj;
}

QJsonObject ProductclassParametrToJson(const ProductparametrClass &par){
    QJsonObject obj;
    obj["id"]=par.id;
    obj["ProductclassID"]=par.ProductclassID;
    obj["parametrID"]=par.parametrID;
    return obj;
}

QJsonObject ProductParameterValueToJson(const ProductParametrValue &par){
    QJsonObject obj;
    obj["id"]=par.id;
    obj["productID"]=par.productID;
    obj["parameterID"]=par.parameterID;
    obj["valueNumber"]=par.valueNumber;
    obj["valueEnumID"]=par.EnumValueID;
    return obj;
}

QJsonObject ParametrToJson(const Parametr &par){
    QJsonObject obj;
    obj["id"]=par.id;
    obj["name"]=par.name;
    obj["type"]=par.type;
    obj["enumID"]=par.enumID;
    obj["unitID"]=par.unitID;
    obj["minValue"]=par.minValue;
    obj["maxValue"]=par.maxValue;
    return obj;
}

QJsonObject TovarToJson(const Product &pro){
    QJsonObject obj;
    obj["id"]=pro.id;
    obj["name"]=pro.name;
    obj["articleNumber"]=pro.articleNumber;
    obj["price"]=pro.price;
    obj["manufacturer"]=pro.manufacturer;
    obj["productclassID"]=pro.productclassID;
    return obj;
}

void sendHttpResponse(QTcpSocket *socket, int statusCode, const QString &statusText,
                      const QString &contentType, const QByteArray &body) {
    QString response = QString("HTTP/1.1 %1 %2\r\n"
                               "Content-Type: %3\r\n"
                               "Content-Length: %4\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Connection: close\r\n"
                               "\r\n")
                           .arg(statusCode)
                           .arg(statusText)
                           .arg(contentType)
                           .arg(body.size());
    socket->write(response.toUtf8() + body);
    socket->flush();
    socket->disconnectFromHost();
}

void sendJsonResponse(QTcpSocket *socket, const QJsonDocument &doc, int status = 200) {
    sendHttpResponse(socket, status, (status == 200) ? "OK" : "Error",
                     "application/json", doc.toJson());
}

void sendErrorResponse(QTcpSocket *socket, const QString &message, int status = 400) {
    QJsonObject obj;
    obj["error"] = message;
    sendJsonResponse(socket, QJsonDocument(obj), status);
}

void sendFileResponse(QTcpSocket *socket, const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        sendErrorResponse(socket, "File not found", 404);
        return;
    }
    QByteArray content = file.readAll();
    QMimeDatabase mimeDb;
    QString mimeType = mimeDb.mimeTypeForFile(filePath).name();
    sendHttpResponse(socket, 200, "OK", mimeType, content);
}

struct HttpRequest {
    QString method;
    QString path;
    QMap<QString, QString> queryParams;
    QByteArray body;
};

HttpRequest parseHttpRequest(const QByteArray &data) {
    HttpRequest req;
    QList<QByteArray> lines = data.split('\r');
    if (lines.isEmpty()) return req;
    QByteArray firstLine = lines[0];
    QList<QByteArray> parts = firstLine.split(' ');
    if (parts.size() >= 2) {
        req.method = QString::fromUtf8(parts[0]);
        QString fullPath = QString::fromUtf8(parts[1]);
        if (fullPath.contains('?')) {
            QStringList pathParts = fullPath.split('?');
            req.path = pathParts[0];
            QUrlQuery query(pathParts[1]);
            for (const auto &pair : query.queryItems())
                req.queryParams[pair.first] = pair.second;
        } else {
            req.path = fullPath;
        }
    }
    int bodyStart = data.indexOf("\r\n\r\n");
    if (bodyStart != -1)
        req.body = data.mid(bodyStart + 4);
    return req;
}

bool parseJsonBody(const QByteArray &body, QJsonObject &obj, QString &errorMsg) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError) {
        errorMsg = "Invalid JSON: " + QString(err.errorString());
        return false;
    }
    if (!doc.isObject()) {
        errorMsg = "JSON must be an object";
        return false;
    }
    obj = doc.object();
    return true;
}

void handleRequest(QTcpSocket *socket, const HttpRequest &req) {
    // Раздача статики веб-интерфейса (если нужен)
    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html")) {
        sendFileResponse(socket, "web/index.html");
        return;
    }
    if (req.method == "GET" && req.path == "/style.css") {
        sendFileResponse(socket, "web/style.css");
        return;
    }
    if (req.method == "GET" && req.path == "/script.js") {
        sendFileResponse(socket, "web/script.js");
        return;
    }

    // === Классы товаров ===
    if (req.method == "GET" && req.path == "/api/classes") {
        QVector<ProductClass> classes = g_db->getAllProductClasses();
        QJsonArray arr;
        for (const auto &c : classes) arr.append(productClassToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if (req.method == "POST" && req.path == "/api/classes") {
        QJsonObject obj;
        QString errMsg;
        if (!parseJsonBody(req.body, obj, errMsg)) {
            sendErrorResponse(socket, errMsg);
            return;
        }
        ProductClass cls;
        cls.code = obj["code"].toString();
        cls.name = obj["name"].toString();
        cls.isTerminal = obj["isTerminal"].toBool();
        cls.baseUnitID = obj["baseUnitID"].toInt();
        cls.parentID = obj["parentID"].toInt();
        cls.orderindex = obj["orderIndex"].toInt();
        if (g_db->AddProductClass(cls))
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status", "ok"}}));
        else
            sendErrorResponse(socket, "Failed to add class (code exists or parent terminal)", 500);
        return;
    }
    if (req.method == "PUT" && req.path == "/api/classes/move") {
        QJsonObject obj;
        QString errMsg;
        if (!parseJsonBody(req.body, obj, errMsg)) {
            sendErrorResponse(socket, errMsg);
            return;
        }
        int classID = obj["classID"].toInt();
        int newParentID = obj["newParentID"].toInt();
        if (g_db->moveProductClass(classID, newParentID))
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status", "moved"}}));
        else
            sendErrorResponse(socket, "Move failed", 500);
        return;
    }
    if (req.method == "PUT" && req.path == "/api/classes/order") {
        QJsonObject obj;
        QString errMsg;
        if (!parseJsonBody(req.body, obj, errMsg)) {
            sendErrorResponse(socket, errMsg);
            return;
        }
        int classID = obj["classID"].toInt();
        int newOrder = obj["orderIndex"].toInt();
        if (g_db->changeOrder(classID, newOrder))
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status", "order changed"}}));
        else
            sendErrorResponse(socket, "Change order failed", 500);
        return;
    }
    if (req.method == "DELETE" && req.path == "/api/classes") {
        if (!req.queryParams.contains("id")) {
            sendErrorResponse(socket, "Missing id parameter");
            return;
        }
        int id = req.queryParams["id"].toInt();
        if (g_db->deleteProductClass(id))
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status", "deleted"}}));
        else
            sendErrorResponse(socket, "Delete failed (has children?)", 500);
        return;
    }
    if (req.method == "PUT" && req.path == "/api/classes/baseunit") {
        QJsonObject obj;
        QString errMsg;
        if (!parseJsonBody(req.body, obj, errMsg)) {
            sendErrorResponse(socket, errMsg);
            return;
        }
        int classID = obj["classID"].toInt();
        int unitID = obj["unitID"].toInt();
        if (g_db->setBaseUnit(classID, unitID))
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status", "base unit set"}}));
        else
            sendErrorResponse(socket, "Set base unit failed", 500);
        return;
    }
    if (req.method == "GET" && req.path == "/api/classes/checkcode") {
        if (!req.queryParams.contains("code")) {
            sendErrorResponse(socket, "Missing code parameter");
            return;
        }
        QString code = req.queryParams["code"];
        bool exists = g_db->classCodeExists(code);
        QJsonObject resp;
        resp["exists"] = exists;
        sendJsonResponse(socket, QJsonDocument(resp));
        return;
    }
    if (req.method == "GET" && req.path == "/api/classes/checkcycle") {
        if (!req.queryParams.contains("classID") || !req.queryParams.contains("parentID")) {
            sendErrorResponse(socket, "Missing classID or parentID");
            return;
        }
        int classID = req.queryParams["classID"].toInt();
        int parentID = req.queryParams["parentID"].toInt();
        bool cycle = g_db->checkCycle(classID, parentID);
        QJsonObject resp;
        resp["cycle"] = cycle;
        sendJsonResponse(socket, QJsonDocument(resp));
        return;
    }
    if (req.method == "GET" && req.path == "/api/classes/child") {
        if (!req.queryParams.contains("id")) {
            sendErrorResponse(socket, "Missing id parameter");
            return;
        }
        int parentID = req.queryParams["id"].toInt();
        QVector<ProductClass> children = g_db->getAllChild(parentID);
        QJsonArray arr;
        for (const auto &c : children) arr.append(productClassToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if (req.method == "GET" && req.path == "/api/classes/parent") {
        if (!req.queryParams.contains("id")) {
            sendErrorResponse(socket, "Missing id parameter");
            return;
        }
        int classID = req.queryParams["id"].toInt();
        QVector<ProductClass> parents = g_db->getAllParents(classID);
        QJsonArray arr;
        for (const auto &c : parents) arr.append(productClassToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if (req.method == "GET" && req.path == "/api/classes/terminal") {
        if (!req.queryParams.contains("id")) {
            sendErrorResponse(socket, "Missing id parameter");
            return;
        }
        int parentID = req.queryParams["id"].toInt();
        QVector<ProductClass> terminals = g_db->getTerminalClasses(parentID);
        QJsonArray arr;
        for (const auto &c : terminals) arr.append(productClassToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }

    // === Перечисления ===
    if(req.method == "GET" && req.path == "/api/classes/Enum"){
        QVector<Enum> Enums = g_db->getEnums();
        QJsonArray arr;
        for (const auto &c : Enums) arr.append(EnumToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if(req.method == "GET" && req.path == "/api/classes/values"){
        if(!req.queryParams.contains("enumID")){
            sendErrorResponse(socket, "Missing enumID parameter");
            return;
        }
        int enumID = req.queryParams["enumID"].toInt();
        QJsonArray arr;
        QVector<EnumValues> val = g_db->getEnumValues(enumID);
        for(const auto &c: val) arr.append(EnumValuesToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if(req.method == "GET" && req.path == "/api/classes/valuesbyid"){
        if(!req.queryParams.contains("id")){
            sendErrorResponse(socket, "Missing id parameter");
            return;
        }
        int id = req.queryParams["id"].toInt();
        QJsonArray arr;
        QVector<EnumValues> val = g_db->getEnumValueByID(id);
        for(const auto &c: val) arr.append(EnumValuesToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if(req.method == "POST" && req.path == "/api/classes/Enum"){
        QJsonObject obj;
        QString err;
        if(!parseJsonBody(req.body,obj,err)){
            sendErrorResponse(socket,err);
            return;
        }
        QString name=obj["name"].toString();
        if(g_db->addEnum(name)){
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status","Enum add"}}));
        }else{
            sendErrorResponse(socket,"Enum don`t add",500);
        }
        return;
    }
    if(req.method == "POST" && req.path == "/api/classes/value"){
        QJsonObject obj;
        QString err;
        if(!parseJsonBody(req.body,obj,err)){
            sendErrorResponse(socket,err);
            return;
        }
        EnumValues val;
        val.id=obj["id"].toInt();
        val.enumid=obj["enumID"].toInt();
        val.code=obj["code"].toString();
        val.orderIndex=obj["orderIndex"].toInt();
        if(g_db->addEnumValue(val)){
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status","EnumValue add"}}));
        }else{
            sendErrorResponse(socket,"EnumValue don`t add",500);
        }
        return;
    }
    if(req.method == "DELETE" && req.path == "/api/classes/Enum"){
        if(!req.queryParams.contains("id")){
            sendErrorResponse(socket, "Missing id parameter");
            return;
        }
        int id = req.queryParams["id"].toInt();
        if (g_db->deleteEnumValue(id))
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status", "deleted"}}));
        else
            sendErrorResponse(socket, "Delete failed (has children?)", 500);
        return;
    }
    if(req.method == "PUT" && req.path == "/api/classes/update"){
        QJsonObject obj;
        QString err;
        if(!parseJsonBody(req.body, obj, err)){
            sendErrorResponse(socket, err);
            return;
        }
        QString newcode= obj["name"].toString();
        int id= obj["id"].toInt();
        if(g_db->updateEnumValue(id,newcode)){
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status"," Code update"}}));
        }else{
            sendErrorResponse(socket, "Code don`t update", 500);
        }
        return;
    }
    if(req.method == "PUT" && req.path == "/api/classes/change"){
        QJsonObject obj;
        QString err;
        if(!parseJsonBody(req.body, obj, err)){
            sendErrorResponse(socket,err);
            return;
        }
        int id=obj["id"].toInt();
        int newOrderIndex=obj["orderIndex"].toInt();
        if(g_db->changeEnumValueOrder(id,newOrderIndex)){
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status","order is change"}}));
        }else{
            sendErrorResponse(socket, "order don`t change", 500);
        }
        return;
    }

    // === Параметры ===
    if(req.method=="GET" && req.path == "/api/classes/param"){
        if(!req.queryParams.contains("classId")){
            sendErrorResponse(socket, "Missing classID parameter");
            return;
        }
        int classID=req.queryParams["classId"].toInt();
        QJsonArray arr;
        QVector<Parametr> par = g_db->getClassParametr(classID);
        for(const auto &c: par) arr.append(ParametrToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if(req.method=="GET" && req.path == "/api/classes/paramval"){
        if(!req.queryParams.contains("productId")){
            sendErrorResponse(socket, "Missing productId parameter");
            return;
        }
        int productId=req.queryParams["productId"].toInt();
        QJsonArray arr;
        QVector<ProductParametrValue> par = g_db->getProductParametrValue(productId);
        for(const auto &c: par) arr.append(ProductParameterValueToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if(req.method=="POST" && req.path == "/api/classes/param"){
        QJsonObject obj;
        QString err;
        if(!parseJsonBody(req.body,obj,err)){
            sendErrorResponse(socket,err);
            return;
        }
        Parametr par;
        par.id=obj["id"].toInt();
        par.name=obj["name"].toString();
        par.type=obj["type"].toString();
        par.enumID=obj["enumID"].toInt();
        par.unitID=obj["unitID"].toInt();
        par.minValue=obj["minValue"].toDouble();
        par.maxValue=obj["maxValue"].toDouble();
        if(g_db->addParametr(par)){
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status","Parametr add"}}));
        }else{
            sendErrorResponse(socket,"Parametr don`t add",500);
        }
        return;
    }
    if(req.method=="POST" && req.path == "/api/classes/paramclass"){
        QJsonObject obj;
        QString err;
        if(!parseJsonBody(req.body,obj,err)){
            sendErrorResponse(socket,err);
            return;
        }
        int ProductclassID=obj["ProductclassID"].toInt();
        int parametrID=obj["parametrID"].toInt();
        if(g_db->addParametrToClass(ProductclassID,parametrID)){
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status","ParametrClass add"}}));
        }else{
            sendErrorResponse(socket,"ParametrClass don`t add",500);
        }
        return;
    }
    if(req.method=="PUT" && req.path == "/api/classes/numberpar"){
        QJsonObject obj;
        QString err;
        if(!parseJsonBody(req.body, obj, err)){
            sendErrorResponse(socket,err);
            return;
        }
        int productID=obj["productID"].toInt();
        int parametrID=obj["parametrID"].toInt();
        int value=obj["value"].toInt();
        if(g_db->setNumberParametrValue(productID,parametrID,value)){
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status","number value set"}}));
        }else{
            sendErrorResponse(socket, "number value don`t set", 500);
        }
        return;
    }
    if(req.method=="PUT" && req.path == "/api/classes/enumpar"){
        QJsonObject obj;
        QString err;
        if(!parseJsonBody(req.body, obj, err)){
            sendErrorResponse(socket,err);
        }
        int productID=obj["productID"].toInt();
        int parametrID=obj["parametrID"].toInt();
        int enumvalueID=obj["enumvalueID"].toInt();
        if(g_db->setEnumParametrValue(productID,parametrID,enumvalueID)){
            sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status","enum value set"}}));
        }else{
            sendErrorResponse(socket, "enum value don`t set", 500);
        }
        return;
    }

    // === Поиск (старые эндпоинты, совместимость) ===
    if(req.method=="GET" && req.path == "/api/classes/findnum"){
        if(!req.queryParams.contains("parameterID") || !req.queryParams.contains("minValue") || !req.queryParams.contains("maxValue")){
            sendErrorResponse(socket, "Missing parameters for search");
            return;
        }
        int parameterID=req.queryParams["parameterID"].toInt();
        double min=req.queryParams["minValue"].toDouble();
        double max=req.queryParams["maxValue"].toDouble();
        int classId = req.queryParams.value("classId", "0").toInt();
        QJsonArray arr;
        QVector<Product> par = g_db->findProductByNumberParam(parameterID, min, max, classId);
        for(const auto &c: par) arr.append(TovarToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if(req.method=="GET" && req.path == "/api/classes/findenum"){
        if(!req.queryParams.contains("parameterID") || !req.queryParams.contains("enumValueID")){
            sendErrorResponse(socket, "Missing parameters for search");
            return;
        }
        int parameterID=req.queryParams["parameterID"].toInt();
        int enumValueID=req.queryParams["enumValueID"].toInt();
        int classId = req.queryParams.value("classId", "0").toInt();
        QJsonArray arr;
        QVector<Product> par = g_db->findProductByEnumParam(parameterID, enumValueID, classId);
        for(const auto &c: par) arr.append(TovarToJson(c));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if(req.method=="GET" && req.path == "/api/classes/findbyprice"){
        if(!req.queryParams.contains("minPrice") || !req.queryParams.contains("maxPrice")){
            sendErrorResponse(socket, "Missing minPrice or maxPrice");
            return;
        }
        double min = req.queryParams["minPrice"].toDouble();
        double max = req.queryParams["maxPrice"].toDouble();
        int classId = req.queryParams.value("classId", "0").toInt();
        QVector<Product> products = g_db->findProductByPrice(min, max, classId);
        QJsonArray arr;
        for (const auto &p : products) arr.append(TovarToJson(p));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }

    // === Новый многопараметрический поиск ===
    if (req.method == "POST" && req.path == "/api/classes/multisearch") {
        QJsonObject body;
        QString err;
        if (!parseJsonBody(req.body, body, err)) {
            sendErrorResponse(socket, err);
            return;
        }
        int classId = body["classId"].toInt();
        QJsonArray conditions = body["conditions"].toArray();
        if (classId == 0 || conditions.isEmpty()) {
            sendErrorResponse(socket, "Invalid request: classId and conditions required");
            return;
        }
        QVector<Product> products = g_db->multiSearch(classId, conditions);
        QJsonArray arr;
        for (const auto &p : products) arr.append(TovarToJson(p));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }

    // === Единицы измерения ===
    if (req.method == "GET" && req.path == "/api/units") {
        QVector<Unit> units = g_db->getAllUnits();
        QJsonArray arr;
        for (const auto &u : units) {
            QJsonObject obj;
            obj["id"] = u.id;
            obj["name"] = u.name;
            obj["shortName"] = u.shortname;
            arr.append(obj);
        }
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    if (req.method == "POST" && req.path == "/api/units") {
        QJsonObject obj;
        QString errMsg;
        if (!parseJsonBody(req.body, obj, errMsg)) {
            sendErrorResponse(socket, errMsg);
            return;
        }
        QString name = obj["name"].toString();
        QString shortName = obj["shortName"].toString();
        if (name.isEmpty() || shortName.isEmpty()) {
            sendErrorResponse(socket, "Name and shortName required");
            return;
        }
        g_db->addUnit(name, shortName);
        sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status", "ok"}}));
        return;
    }
    if (req.method == "DELETE" && req.path == "/api/units") {
        if (!req.queryParams.contains("id")) {
            sendErrorResponse(socket, "Missing id parameter");
            return;
        }
        int id = req.queryParams["id"].toInt();
        g_db->deleteUnit(id);
        sendJsonResponse(socket, QJsonDocument(QJsonObject{{"status", "deleted"}}));
        return;
    }
    if (req.method == "POST" && req.path == "/api/classes/multisearch") {
        QJsonObject body;
        QString err;
        if (!parseJsonBody(req.body, body, err)) {
            sendErrorResponse(socket, err);
            return;
        }
        int classId = body["classId"].toInt();
        QJsonArray conditions = body["conditions"].toArray();
        if (classId == 0) {
            sendErrorResponse(socket, "classId required");
            return;
        }
        QVector<Product> products = g_db->multiSearch(classId, conditions);
        QJsonArray arr;
        for (const auto &p : products) arr.append(TovarToJson(p));
        sendJsonResponse(socket, QJsonDocument(arr));
        return;
    }
    sendErrorResponse(socket, "Not found", 404);
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    g_db = new Database();
    g_db->connectToDatabase();
    QTcpServer server;
    if (!server.listen(QHostAddress::Any, 8080)) {
        qCritical() << "Не удалось запустить сервер на порту 8080";
        return -1;
    }
    qDebug() << "Сервер запущен на порту 8080";
    QObject::connect(&server, &QTcpServer::newConnection, [&]() {
        QTcpSocket *socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, [socket]() {
            QByteArray data = socket->readAll();
            HttpRequest req = parseHttpRequest(data);
            handleRequest(socket, req);
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    });
    return a.exec();
}
