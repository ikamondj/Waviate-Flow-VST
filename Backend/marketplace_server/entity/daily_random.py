from sqlalchemy import Column, Integer, String
from db import Base

class DailyRandom(Base):
    __tablename__ = 'daily_random'
    minute = Column(Integer, primary_key=True)  # 0..1439
    entry_ids = Column(String, nullable=False)  # comma-separated list of entry UUIDs
