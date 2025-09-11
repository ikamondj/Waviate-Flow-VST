from sqlalchemy import Column, BigInteger, String, DateTime, Boolean
from sqlalchemy.ext.declarative import declarative_base

Base = declarative_base()

class MarketEntry(Base):
    __tablename__ = 'marketplace_content'
    nodeId = Column(BigInteger, primary_key=True)
    userId = Column(BigInteger, primary_key=True)
    name = Column(String)
    description = Column(String)
    created_at = Column(DateTime)
    updated_at = Column(DateTime)
    isArchived = Column(Boolean, default=False)

    # Add other fields as needed
