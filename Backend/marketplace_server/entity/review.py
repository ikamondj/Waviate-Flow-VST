from sqlalchemy import Column, String, BigInteger, Text, TIMESTAMP, ForeignKey
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import relationship
import uuid
from db import Base

class Review(Base):
    __tablename__ = 'reviews'
    review_id = Column(String, primary_key=True, default=lambda: str(uuid.uuid4()))
    user_id = Column(BigInteger, nullable=False)
    entry_user_id = Column(BigInteger, nullable=False)
    entry_node_id = Column(BigInteger, nullable=False)
    comment = Column(Text)
    created_at = Column(TIMESTAMP, server_default="now()")
    star_ratings = relationship("ReviewStarRating", back_populates="review", cascade="all, delete-orphan")
