from sqlalchemy import Column, String
from sqlalchemy.dialects.postgresql import UUID
from db import Base

class Category(Base):
    __tablename__ = 'categories'
    id = Column(UUID(as_uuid=True), primary_key=True)
    name = Column(String, unique=True, nullable=False)
