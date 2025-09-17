from sqlalchemy import Column, String, BigInteger, DateTime, ForeignKey
from sqlalchemy.dialects.postgresql import UUID
from db import Base

class Subscription(Base):
    __tablename__ = 'subscriptions'
    id = Column(UUID(as_uuid=True), primary_key=True)
    user_id = Column(BigInteger, ForeignKey('users.id', ondelete='CASCADE'), nullable=False)
    status = Column(String, nullable=False)
    plan = Column(String, nullable=False, default='pro')
    provider = Column(String)
    external_id = Column(String)
    started_at = Column(DateTime)
    renews_at = Column(DateTime)
    canceled_at = Column(DateTime)
