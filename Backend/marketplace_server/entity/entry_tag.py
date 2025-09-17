from sqlalchemy import Column, UUID, ForeignKey
from db import Base

class EntryTag(Base):
    __tablename__ = 'entry_tags'
    entry_id = Column(UUID(as_uuid=True), ForeignKey('entries.id', ondelete='CASCADE'), primary_key=True)
    tag_id = Column(UUID(as_uuid=True), ForeignKey('tags.id', ondelete='CASCADE'), primary_key=True)
