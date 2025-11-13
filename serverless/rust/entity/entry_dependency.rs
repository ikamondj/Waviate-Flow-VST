use sea_orm::entity::prelude::*;
use sqlx::FromRow;

#[derive(Clone, Debug, PartialEq, DeriveEntityModel, FromRow)]
#[sea_orm(table_name = "entry_dependencies")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub entry_user_id: i64,
    #[sea_orm(primary_key)]
    pub entry_node_id: i64,
    #[sea_orm(primary_key)]
    pub depends_on_user_id: i64,
    #[sea_orm(primary_key)]
    pub depends_on_node_id: i64,
}

#[derive(Copy, Clone, Debug, EnumIter)]
pub enum Relation {
    #[sea_orm(
        belongs_to = "super::marketplace_content::Entity",
        from = "Column::EntryUserId",
        to = "super::marketplace_content::Column::UserId"
    )]
    DependeeUser,

    #[sea_orm(
        belongs_to = "super::marketplace_content::Entity",
        from = "Column::EntryNodeId",
        to = "super::marketplace_content::Column::NodeId"
    )]
    DependeeNode,

    #[sea_orm(
        belongs_to = "super::marketplace_content::Entity",
        from = "Column::DependsOnUserId",
        to = "super::marketplace_content::Column::UserId"
    )]
    DependerUser,

    #[sea_orm(
        belongs_to = "super::marketplace_content::Entity",
        from = "Column::DependsOnNodeId",
        to = "super::marketplace_content::Column::NodeId"
    )]
    DependerNode,
}

impl Related<super::marketplace_content::Entity> for Entity {
    fn to() -> RelationDef {
        Relation::DependeeUser.def()
    }

    fn via() -> Option<RelationDef> {
        Some(Relation::DependeeNode.def())
    }
}

impl ActiveModelBehavior for ActiveModel {}