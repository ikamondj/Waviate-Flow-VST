use sea_orm::entity::prelude::*;
use sqlx::FromRow;

#[derive(Clone, Debug, PartialEq, DeriveEntityModel, FromRow)]
#[sea_orm(table_name = "entry_tags")]
pub struct Model {
    #[sea_orm(primary_key)]
    pub entry_id: Uuid,
    #[sea_orm(primary_key)]
    pub tag_id: Uuid,
}

#[derive(Copy, Clone, Debug, EnumIter)]
pub enum Relation {
    #[sea_orm(
        belongs_to = "super::marketplace_content::Entity",
        from = "Column::EntryId",
        to = "super::marketplace_content::Column::NodeId"
    )]
    MarketplaceContent,
}

impl Related<super::marketplace_content::Entity> for Entity {
    fn to() -> RelationDef {
        Relation::MarketplaceContent.def()
    }
}

impl ActiveModelBehavior for ActiveModel {}