use sea_orm::entity::prelude::*;
use sqlx::FromRow;

#[derive(Clone, Debug, PartialEq, DeriveEntityModel, FromRow)]
#[sea_orm(table_name = "reviews")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub review_id: String,
    pub user_id: i64,
    pub entry_user_id: i64,
    pub entry_node_id: i64,
    pub comment: Option<String>,
    pub created_at: DateTime,
}

#[derive(Copy, Clone, Debug, EnumIter)]
pub enum Relation {}

impl ActiveModelBehavior for ActiveModel {}