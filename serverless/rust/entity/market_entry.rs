use sea_orm::entity::prelude::*;
use sqlx::FromRow;

#[derive(Clone, Debug, PartialEq, DeriveEntityModel, FromRow)]
#[sea_orm(table_name = "marketplace_content")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub node_id: i64,
    #[sea_orm(primary_key)]
    pub user_id: i64,

    pub name: Option<String>,
    pub description: Option<String>,
    pub created_at: Option<DateTime>,
    pub updated_at: Option<DateTime>,
    pub is_archived: bool,
}

#[derive(Copy, Clone, Debug, EnumIter)]
pub enum Relation {
    #[sea_orm(
        belongs_to = "super::users::Entity",
        from = "Column::UserId",
        to = "super::users::Column::Id"
    )]
    User,
}

impl Related<super::users::Entity> for Entity {
    fn to() -> RelationDef {
        Relation::User.def()
    }
}

impl ActiveModelBehavior for ActiveModel {}
