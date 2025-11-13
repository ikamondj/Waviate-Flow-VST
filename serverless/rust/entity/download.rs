use sea_orm::entity::prelude::*;
use sqlx::FromRow;

#[derive(Clone, Debug, PartialEq, DeriveEntityModel, FromRow)]
#[sea_orm(table_name = "downloads")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub user_id: i64,
    #[sea_orm(primary_key)]
    pub entry_user_id: i64,
    #[sea_orm(primary_key)]
    pub entry_node_id: i64,
    pub subscribed_at: Option<DateTime>,
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
        from = "Column::EntryUserId",
        to = "super::marketplace_content::Column::UserId"
    )]
    EntryUser,

    #[sea_orm(
        belongs_to = "super::marketplace_content::Entity",
        from = "Column::EntryNodeId",
        to = "super::marketplace_content::Column::NodeId"
    )]
    EntryNode,
}

impl ActiveModelBehavior for ActiveModel {}

impl Related<super::users::Entity> for Entity {
    fn to() -> RelationDef {
        Relation::User.def()
    }
}

impl Related<super::marketplace_content::Entity> for Entity {
    fn to() -> RelationDef {
        Relation::EntryUser.def()
    }

    fn via() -> Option<RelationDef> {
        Some(Relation::EntryNode.def())
    }
}