from sqlalchemy import Column, String, Integer, ForeignKey
from sqlalchemy.orm import relationship
from db import Base

class ReviewStarRating(Base):
    __tablename__ = 'review_star_ratings'
    review_id = Column(String, ForeignKey('reviews.review_id', ondelete='CASCADE'), primary_key=True)
    rating_type_id = Column(Integer, ForeignKey('star_rating_types.rating_type_id'), primary_key=True)
    stars = Column(Integer, nullable=False)
    review = relationship("Review", back_populates="star_ratings")
    rating_type = relationship("StarRatingType")
