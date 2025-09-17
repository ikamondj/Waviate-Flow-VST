from sqlalchemy import Column, UUID, BigInteger, DateTime
from db import Base

class Download(Base):
    __tablename__ = 'downloads'
    id = Column(UUID(as_uuid=True), primary_key=True)
    user_id = Column(BigInteger, nullable=False)
    entry_user_id = Column(BigInteger, nullable=False)
    entry_node_id = Column(BigInteger, nullable=False)
    downloaded_at = Column(DateTime)
