from sqlalchemy import Column, BigInteger, Integer
from db import Base

class UserEntrySelection(Base):
    __tablename__ = 'user_entry_selections'
    user_id = Column(BigInteger, primary_key=True)
    entry_user_id = Column(BigInteger, primary_key=True)
    entry_node_id = Column(BigInteger, primary_key=True)
    selection_type = Column(Integer, nullable=False)
    created_at = Column(Integer)
