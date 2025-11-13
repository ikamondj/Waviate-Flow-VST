use sea_orm::entity::prelude::*;
use sqlx::FromRow;

#[derive(Clone, Debug, PartialEq, DeriveEntityModel, FromRow)]
#[sea_orm(table_name = "user_entry_selections")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub user_id: i64,
    #[sea_orm(primary_key)]
    pub entry_user_id: i64,
    #[sea_orm(primary_key)]
    pub entry_node_id: i64,
    pub selected_at: DateTime,
}

#[derive(Copy, Clone, Debug, EnumIter)]
pub enum Relation {}

impl ActiveModelBehavior for ActiveModel {}