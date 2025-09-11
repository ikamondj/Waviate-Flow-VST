from sqlalchemy import Column, Integer, String
from sqlalchemy.ext.declarative import declarative_base

Base = declarative_base()

class RandomList(Base):
    __tablename__ = 'random_daily'
    minute = Column(Integer, primary_key=True)
    entry_ids = Column(String)  # Comma-separated list of nodeId:userId pairs
