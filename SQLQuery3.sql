Create table parametr(
 id INTEGER IDENTITY(1,1) PRIMARY KEY,
 name NVARCHAR(50) NOT NULL,
 type NVARCHAR(20) NOT NULL CHECK,
 enumID INTEGER NULL,
 unitID INTEGER NULL,
 minValue DECIMAL(10,2) NULL,
 maxValue DECIMAL(10,2) NULL 
)

Create table ProductclassParametr(
id INTEGER IDENTITY(1,1) PRIMARY KEY,
ProductclassID INTEGER NOT NULL,
parametrID INTEGER NOT NULL,
Constraint FK_parametrclass_productclass FOREIGN KEY (ProductclassID) REFERENCES ProductClass(id),
Constraint FK_parametrclass_parametr FOREIGN KEY (parametrID) REFERENCES parametr(id)
)

CREATE TABLE ProductParameterValue(
 id INT IDENTITY(1,1) PRIMARY KEY,
 productID INT NOT NULL,
 parameterID INT NOT NULL,
 valueNumber DECIMAL(10,2) NULL,
 valueEnumID INT NULL,
 CONSTRAINT FK_PPV_Product FOREIGN KEY(productID)
  REFERENCES tovar(id),
 CONSTRAINT FK_PPV_Param FOREIGN KEY(parameterID)
  REFERENCES Parameter(id),
 CONSTRAINT FK_PPV_EnumValue FOREIGN KEY(valueEnumID)
  REFERENCES EnumValues(id)
);