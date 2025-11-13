use sea_orm::entity::prelude::*;
use sqlx::FromRow;
use uuid::Uuid;

#[derive(Clone, Debug, PartialEq, DeriveEntityModel)]
#[sea_orm(table_name = "tags")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub id: Uuid,
    pub name: String,
}

#[derive(Copy, Clone, Debug, EnumIter)]
pub enum Relation {}

impl ActiveModelBehavior for ActiveModel {}

#[derive(Debug, FromRow)]
pub struct Tag {
    pub id: Uuid,
    pub name: String,
}