from db import get_db
from sqlalchemy.exc import IntegrityError
from entity.market_entry import MarketEntry
from entity.user import User
from entity.random_list import RandomList
from sqlalchemy import select
import uuid
from entity.review import Review
from entity.review_star_rating import ReviewStarRating
from entity.star_rating_type import StarRatingType

# Assume: review_star_ratings, reviews, star_rating_types are mapped with SQLAlchemy ORM elsewhere

def create_review(user_id: int, entry_user_id: int, entry_node_id: int, comment: str, ratings: dict):
    """
    Create a review for a marketplace entry using ORM.
    - user_id: reviewer
    - entry_user_id, entry_node_id: identifies the entry
    - comment: review text
    - ratings: dict of {rating_type_id: stars (1-5)}
    """
    if not user_id or not entry_user_id or not entry_node_id or not comment or not ratings:
        return {"error": "Missing parameters", "code": 400}
    if not isinstance(ratings, dict) or not (1 <= len(ratings) <= 5):
        return {"error": "Must provide 1-5 rating types", "code": 400}
    for v in ratings.values():
        if not (1 <= v <= 5):
            return {"error": "Ratings must be 1-5 stars", "code": 400}
    db = next(get_db())
    # Check if user already reviewed this entry
    existing = db.query(Review).filter_by(user_id=user_id, entry_user_id=entry_user_id, entry_node_id=entry_node_id).first()
    if existing:
        return {"error": "User has already reviewed this entry", "code": 409}
    # Create review
    review = Review(user_id=user_id, entry_user_id=entry_user_id, entry_node_id=entry_node_id, comment=comment)
    db.add(review)
    db.flush()  # Assigns review_id
    # Insert star ratings
    for rating_type_id, stars in ratings.items():
        # Optionally, check if rating_type_id exists
        if not db.query(StarRatingType).filter_by(rating_type_id=rating_type_id).first():
            db.rollback()
            return {"error": f"Invalid rating_type_id: {rating_type_id}", "code": 400}
        star_rating = ReviewStarRating(review_id=review.review_id, rating_type_id=rating_type_id, stars=stars)
        db.add(star_rating)
    db.commit()
    return {"message": "Review created", "review_id": review.review_id}

def update_review(user_id: int, review_id: str, comment: str = None, ratings: dict = None):
    """
    Update a review's comment and/or star ratings. Only the author can update.
    - user_id: the user attempting the update
    - review_id: the review to update
    - comment: new comment (optional)
    - ratings: dict of {rating_type_id: stars (1-5)} (optional)
    """
    if not user_id or not review_id:
        return {"error": "Missing parameters", "code": 400}
    db = next(get_db())
    review = db.query(Review).filter_by(review_id=review_id).first()
    if not review:
        return {"error": "Review not found", "code": 404}
    if review.user_id != user_id:
        return {"error": "Forbidden: not review author", "code": 403}
    updated = False
    if comment is not None:
        review.comment = comment
        updated = True
    if ratings is not None:
        if not isinstance(ratings, dict) or not (1 <= len(ratings) <= 5):
            return {"error": "Must provide 1-5 rating types", "code": 400}
        for v in ratings.values():
            if not (1 <= v <= 5):
                return {"error": "Ratings must be 1-5 stars", "code": 400}
        # Remove old ratings
        db.query(ReviewStarRating).filter_by(review_id=review_id).delete()
        # Add new ratings
        for rating_type_id, stars in ratings.items():
            if not db.query(StarRatingType).filter_by(rating_type_id=rating_type_id).first():
                db.rollback()
                return {"error": f"Invalid rating_type_id: {rating_type_id}", "code": 400}
            star_rating = ReviewStarRating(review_id=review_id, rating_type_id=rating_type_id, stars=stars)
            db.add(star_rating)
        updated = True
    if updated:
        db.commit()
        return {"message": "Review updated", "review_id": review_id}
    else:
        return {"error": "No changes provided", "code": 400}

def delete_review(user_id: int, review_id: str):
    """
    Delete a review. Only the author can delete.
    """
    if not user_id or not review_id:
        return {"error": "Missing parameters", "code": 400}
    db = next(get_db())
    review = db.query(Review).filter_by(review_id=review_id).first()
    if not review:
        return {"error": "Review not found", "code": 404}
    if review.user_id != user_id:
        return {"error": "Forbidden: not review author", "code": 403}
    db.delete(review)
    db.commit()
    return {"message": "Review deleted", "review_id": review_id}


def list_reviews_by_entry(entry_user_id: int, entry_node_id: int):
    """
    List all reviews for a given entry, including comment, reviewer, and up to 5 star ratings.
    """
    db = next(get_db())
    reviews = db.query(Review).filter_by(entry_user_id=entry_user_id, entry_node_id=entry_node_id).all()
    result = []
    for review in reviews:
        ratings = db.query(ReviewStarRating, StarRatingType).join(StarRatingType, ReviewStarRating.rating_type_id == StarRatingType.rating_type_id).filter(ReviewStarRating.review_id == review.review_id).all()
        ratings_list = [
            {"type": rtype.name, "stars": rsr.stars, "rating_type_id": rsr.rating_type_id}
            for rsr, rtype in ratings
        ]
        result.append({
            "review_id": review.review_id,
            "user_id": review.user_id,
            "comment": review.comment,
            "created_at": str(review.created_at),
            "star_ratings": ratings_list
        })
    return {"reviews": result}
