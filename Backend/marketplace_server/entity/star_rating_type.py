from sqlalchemy import Column, Integer, String
from db import Base

class StarRatingType(Base):
    __tablename__ = 'star_rating_types'
    rating_type_id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String, unique=True, nullable=False)
