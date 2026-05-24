CREATE TABLE Aggregate (
    id INT IDENTITY(1,1) PRIMARY KEY,
    name NVARCHAR(200) NOT NULL,
    description NVARCHAR(500) NULL
);

CREATE TABLE AggregateParameter (
    aggregateID INT NOT NULL,
    parametrID INT NOT NULL,
    orderIndex INT DEFAULT 0,
    CONSTRAINT PK_AggregateParametr PRIMARY KEY (aggregateID, parametrID),
    CONSTRAINT FK_AP_Aggregate FOREIGN KEY (aggregateID) REFERENCES Aggregate(id),
    CONSTRAINT FK_AP_Parametr FOREIGN KEY (parametrID) REFERENCES parametr(id)
);