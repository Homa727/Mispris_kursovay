insert into parametr(name,type,enumID,unitID, maxValue, minValue) values('Диаметр','number', NULL, 1, 10, 200),
('Длина','number', NULL, 1, 10, 500),
('Тип двигателя','enum', 1, NULL, NULL, NULL),
('Тип тормозов','enum', 2, NULL, NULL, NULL);

insert into ProductclassParametr(ProductclassID, parametrID) values(2,3),
(4,1),(4,2),(5,1),(3,4),(6,4),(7,1),(7,4);

insert into ProductParameterValue(productID, parameterID, valueNumber) values(1, 1, 84),
(1, 2, 75),(2, 1, 82),(2, 2, 74);

insert into ProductParameterValue(productID, parameterID, valueNumber) values(3, 1, 60),
(4, 1, 65);
insert into ProductParameterValue(productID, parameterID, valueEnumID) values(5, 4, 3),
(6, 4, 3),(7, 4, 3),(8, 4, 3);
insert into ProductParameterValue(productID, parameterID, valueNumber) values(7, 1, 300),
(8, 1, 280);

INSERT INTO OperationClass(name, description) VALUES('Отгрузка','Операции отгрузки товаров'),
('Поставка','Операции поставки товаров'), ('Возврат','Операции возврата товара');

INSERT INTO OperationTemplate(classID,name,description) VALUES(1,'Шаблон накладной','Типовая товарная наклодная'),(2,'Шаблон поставки','Международная поставка'),(3,'шаблон возврата','Документ возврата товара');

INSERT INTO Operation(templateID, operationDate, status) VALUES(1, GETDATE(),'Создана'),(2,GETDATE(),'Подтверждена'),(3,GETDATE(),'Закрыта');

INSERT INTO OperationRole(operationID, roleName, participantName) VALUES(1, 'Поставщик','ООО Хома'),(2,'Получатель','Дерево'),(3,'Перевозчик','Тожман');

INSERT INTO OperationParameterValue(
    operationID,
    parameterID,
    numberValue,
    enumValueID,
    stringValue
)
VALUES
(
 1,
 1,
 150.50,
 NULL,
 NULL
),

(
 2,
 3,
 NULL,
 2,
 NULL
),

(
 3,
 1,
 75.25,
 NULL,
 'Поврежденный товар'
); 

INSERT INTO


OperationDocument(
    operationID,
    documentType,
    documentNumber,
    documentDate
)
VALUES
(
 1,
 'Накладная',
 'NK-001',
 GETDATE()
),

(
 2,
 'Invoice',
 'INV-002',
 GETDATE()
),

(
 3,
 'Акт возврата',
 'RET-003',
 GETDATE()
);