-- Классификатор типов хозяйственных операций
CREATE TABLE BusinessOperationType (
    id INT IDENTITY(1,1) PRIMARY KEY,
    code NVARCHAR(50) NOT NULL UNIQUE,
    name NVARCHAR(200) NOT NULL,
    description NVARCHAR(500) NULL
);

-- Шаблон ХО: определяет состав параметров и ролей для данного типа
CREATE TABLE OperationTemplate (
    id INT IDENTITY(1,1) PRIMARY KEY,
    opTypeID INT NOT NULL,
    name NVARCHAR(200) NOT NULL,                -- например "Отгрузка по ТТН"
    isActive BIT DEFAULT 1,
    CONSTRAINT FK_Template_OpType FOREIGN KEY (opTypeID) REFERENCES BusinessOperationType(id)
);

-- Параметры шаблона (числовые, строковые, дата, enum)
CREATE TABLE TemplateParameter (
    id INT IDENTITY(1,1) PRIMARY KEY,
    templateID INT NOT NULL,
    name NVARCHAR(100) NOT NULL,                -- "Сумма", "Вес", "Количество мест"
    paramType NVARCHAR(20) NOT NULL CHECK (paramType IN ('NUMBER','STRING','DATE','ENUM')),
    isRequired BIT DEFAULT 1,
    enumID INT NULL,                            -- ссылка на Enum (если тип ENUM)
    unitID INT NULL,                            -- единица измерения (опционально)
    minValue DECIMAL(18,4) NULL,                -- ограничения для числовых
    maxValue DECIMAL(18,4) NULL,
    orderIndex INT DEFAULT 0,
    CONSTRAINT FK_TP_Template FOREIGN KEY (templateID) REFERENCES OperationTemplate(id),
    CONSTRAINT FK_TP_Enum FOREIGN KEY (enumID) REFERENCES Enum(id),
    CONSTRAINT FK_TP_Unit FOREIGN KEY (unitID) REFERENCES Units(id)
);

-- Роли участников/объектов ХО (грузоотправитель, грузополучатель, водитель, ТС и т.п.)
CREATE TABLE TemplateRole (
    id INT IDENTITY(1,1) PRIMARY KEY,
    templateID INT NOT NULL,
    roleCode NVARCHAR(50) NOT NULL,             -- "SHIPPER", "CONSIGNEE", "CARRIER"
    roleName NVARCHAR(200) NOT NULL,
    isMultiple BIT DEFAULT 0,                   -- можно ли несколько значений
    orderIndex INT DEFAULT 0,
    CONSTRAINT FK_TR_Template FOREIGN KEY (templateID) REFERENCES OperationTemplate(id)
);

-- Экземпляр ХО (конкретная операция)
CREATE TABLE BusinessOperation (
    id INT IDENTITY(1,1) PRIMARY KEY,
    templateID INT NOT NULL,
    opNumber NVARCHAR(50) NOT NULL,             -- номер документа
    opDate DATE NOT NULL,
    status NVARCHAR(20) DEFAULT 'DRAFT',        -- DRAFT, POSTED, CANCELLED
    createdBy NVARCHAR(100),
    createdDt DATETIME DEFAULT GETDATE(),
    CONSTRAINT FK_BO_Template FOREIGN KEY (templateID) REFERENCES OperationTemplate(id)
);

-- Значения параметров для экземпляра
CREATE TABLE OperationParameterValue (
    id INT IDENTITY(1,1) PRIMARY KEY,
    operationID INT NOT NULL,
    parameterID INT NOT NULL,                   -- ссылка на TemplateParameter
    valueNumber DECIMAL(18,4) NULL,
    valueString NVARCHAR(500) NULL,
    valueDate DATE NULL,
    valueEnumID INT NULL,                       -- если параметр типа ENUM
    CONSTRAINT FK_OPV_Operation FOREIGN KEY (operationID) REFERENCES BusinessOperation(id),
    CONSTRAINT FK_OPV_Param FOREIGN KEY (parameterID) REFERENCES TemplateParameter(id),
    CONSTRAINT FK_OPV_EnumValue FOREIGN KEY (valueEnumID) REFERENCES EnumValues(id)
);

-- Назначения на роли (ссылки на сущности: контрагенты, сотрудники, транспорт)
-- Для простоты можно хранить ID сущностей из разных справочников плюс тип.
CREATE TABLE OperationRoleAssignment (
    id INT IDENTITY(1,1) PRIMARY KEY,
    operationID INT NOT NULL,
    roleID INT NOT NULL,                        -- ссылка на TemplateRole
    entityType NVARCHAR(50) NOT NULL,           -- 'Counterparty', 'Employee', 'Vehicle'
    entityID INT NOT NULL,                      -- ID в соответствующей таблице
    CONSTRAINT FK_ORA_Operation FOREIGN KEY (operationID) REFERENCES BusinessOperation(id),
    CONSTRAINT FK_ORA_Role FOREIGN KEY (roleID) REFERENCES TemplateRole(id)
);