use sea_orm::entity::prelude::*;
use sqlx::FromRow;

#[derive(Clone, Debug, PartialEq, DeriveEntityModel, FromRow)]
#[sea_orm(table_name = "daily_random")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub minute: i32,
    #[sea_orm(primary_key)]
    pub user_id: i64,
    #[sea_orm(primary_key)]
    pub entry_id: i64,
}

#[derive(Copy, Clone, Debug, EnumIter)]
pub enum Relation {
    #[sea_orm(
        belongs_to = "super::users::Entity",
        from = "Column::UserId",
        to = "super::users::Column::Id"
    )]
    User,

    #[sea_orm(
        belongs_to = "super::marketplace_content::Entity",
        from = "Column::EntryId",
        to = "super::marketplace_content::Column::NodeId"
    )]
    MarketplaceContent,
}

impl Related<super::users::Entity> for Entity {
    fn to() -> RelationDef {
        Relation::User.def()
    }
}

impl Related<super::marketplace_content::Entity> for Entity {
    fn to() -> RelationDef {
        Relation::MarketplaceContent.def()
    }
}

impl ActiveModelBehavior for ActiveModel {}