from sqlalchemy import create_engine, text
from sqlalchemy.engine import Engine, Result
import os

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

def execute_query(sql: str, params: dict = None) -> Result:
    """Execute a SQL query and return the result."""
    with engine.connect() as conn:
        result = conn.execute(text(sql), params or {})
        return result

def fetch_all(sql: str, params: dict = None):
    """Fetch all rows from a query."""
    result = execute_query(sql, params)
    return result.fetchall()

def fetch_one(sql: str, params: dict = None):
    """Fetch one row from a query."""
    result = execute_query(sql, params)
    return result.fetchone()
