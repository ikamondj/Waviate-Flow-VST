use sea_orm::entity::prelude::*;
use sqlx::FromRow;

#[derive(Clone, Debug, PartialEq, DeriveEntityModel, FromRow)]
#[sea_orm(table_name = "review_star_ratings")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub review_id: String,
    #[sea_orm(primary_key)]
    pub rating_type_id: i32,
    pub stars: i32,
}

#[derive(Copy, Clone, Debug, EnumIter)]
pub enum Relation {}

impl ActiveModelBehavior for ActiveModel {}