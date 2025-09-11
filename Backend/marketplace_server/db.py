from sqlalchemy import create_engine, text
from sqlalchemy.engine import Engine, Result
from sqlalchemy.orm import sessionmaker, scoped_session, declarative_base
import os
from entity.market_entry import MarketEntry

# Example: 'cockroachdb://username:password@host:port/database'
DATABASE_URL = os.getenv('DATABASE_URL', 'cockroachdb://user:pass@localhost:26257/defaultdb')

# For serverless, use pool_pre_ping=True and pool_size=1 to avoid pooling issues
def get_engine() -> Engine:
    return create_engine(
        DATABASE_URL,
        pool_pre_ping=True,
        pool_size=1,
        max_overflow=0,
        echo=False,
        connect_args={
            # CockroachDB/Postgres SSL options can go here if needed
        }
    )

engine = get_engine()

# Centralized declarative base for all models
Base = declarative_base()

# Import all models so they are registered with Base.metadata
try:
    from entity.market_entry import MarketEntry
    from entity.user import User
    from entity.random_list import RandomList
except ImportError:
    pass  # Allow import even if entities not yet defined

# Session factory for ORM usage
SessionLocal = scoped_session(sessionmaker(autocommit=False, autoflush=False, bind=engine))

def get_db():
    """Yield a SQLAlchemy session, closing it after use."""
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

def execute_query(sql: str, params: dict = None) -> Result:
    """Execute a SQL query and return the result."""
    with engine.connect() as conn:
        result = conn.execute(text(sql), params or {})
        return result

def fetch_all(sql: str, params: dict = None):
    """Fetch all rows from a query."""
    result = execute_query(sql, params)
    return result.fetchall()

def truncate_table(table_name: str):
    """Truncate a table."""
    sql = f"TRUNCATE TABLE {table_name}"
    execute_query(sql)

def get_all_marketplace_entries():
    """Fetch all entries from the marketplace_content table."""
    with next(get_db()) as db:
        return db.query(MarketEntry).all()