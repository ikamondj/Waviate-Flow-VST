use sqlx::{Pool, Executor, FromRow, Error, query_as, query};
use sqlx::postgres::Postgres;

pub struct Database {
    pool: Pool<Postgres>,
}

impl Database {
    pub async fn new(database_url: &str) -> Result<Self, Error> {
        let pool = Pool::<Postgres>::connect(database_url).await?;
        Ok(Self { pool })
    }

    pub async fn execute(&self, query: &str, params: &[&(dyn sqlx::Encode<'_> + sqlx::Type)]) -> Result<u64, Error> {
        let result = sqlx::query(query)
            .bind_all(params)
            .execute(&self.pool)
            .await?;
        Ok(result.rows_affected())
    }

    pub async fn fetch_one<T>(&self, query: &str, params: &[&(dyn sqlx::Encode<'_> + sqlx::Type)]) -> Result<T, Error>
    where
        T: for<'r> FromRow<'r, sqlx::postgres::PgRow>,
    {
        let result = query_as::<_, T>(query)
            .bind_all(params)
            .fetch_one(&self.pool)
            .await?;
        Ok(result)
    }

    pub async fn fetch_all<T>(&self, query: &str, params: &[&(dyn sqlx::Encode<'_> + sqlx::Type)]) -> Result<Vec<T>, Error>
    where
        T: for<'r> FromRow<'r, sqlx::postgres::PgRow>,
    {
        let result = query_as::<_, T>(query)
            .bind_all(params)
            .fetch_all(&self.pool)
            .await?;
        Ok(result)
    }

    pub async fn transaction<F, T>(&self, func: F) -> Result<T, Error>
    where
        F: FnOnce(&mut sqlx::Transaction<'_, Postgres>) -> T + Send,
        T: Send,
    {
        let mut transaction = self.pool.begin().await?;
        let result = func(&mut transaction);
        transaction.commit().await?;
        Ok(result)
    }
}

trait BindAll<'q> {
    fn bind_all(self, params: &[&(dyn sqlx::Encode<'q> + sqlx::Type)]) -> Self;
}

impl<'q> BindAll<'q> for sqlx::query::Query<'q, Postgres, sqlx::postgres::PgArguments> {
    fn bind_all(mut self, params: &[&(dyn sqlx::Encode<'q> + sqlx::Type)]) -> Self {
        for param in params {
            self = self.bind(*param);
        }
        self
    }
}
