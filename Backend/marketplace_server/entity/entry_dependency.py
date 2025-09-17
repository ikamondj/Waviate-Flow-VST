from sqlalchemy import Column, BigInteger, ForeignKey
from db import Base

class EntryDependency(Base):
    __tablename__ = 'entry_dependencies'
    entry_user_id = Column(BigInteger, primary_key=True)
    entry_node_id = Column(BigInteger, primary_key=True)
    depends_on_user_id = Column(BigInteger, primary_key=True)
    depends_on_node_id = Column(BigInteger, primary_key=True)
