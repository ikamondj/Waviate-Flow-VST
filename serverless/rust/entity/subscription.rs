use sea_orm::entity::prelude::*;
use sqlx::FromRow;
use uuid::Uuid;
use chrono::{NaiveDateTime, DateTime};

#[derive(Clone, Debug, PartialEq, DeriveEntityModel, FromRow)]
#[sea_orm(table_name = "subscriptions")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub id: Uuid,
    pub user_id: i64,
    pub status: String,
    pub plan: String,
    pub provider: Option<String>,
    pub external_id: Option<String>,
    pub started_at: Option<DateTime>,
    pub renews_at: Option<DateTime>,
    pub canceled_at: Option<DateTime>,
}

#[derive(Copy, Clone, Debug, EnumIter)]
pub enum Relation {}

impl ActiveModelBehavior for ActiveModel {}