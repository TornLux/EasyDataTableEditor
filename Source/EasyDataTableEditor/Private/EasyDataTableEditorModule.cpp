// Copyright Epic Games, Inc. All Rights Reserved.

#include "EasyDataTableEditorModule.h"

#include "ContentBrowserMenuContexts.h"
#include "EasyCompositeDataTableEditor.h"
#include "EasyDataTableEditor.h"
#include "Engine/CompositeDataTable.h"

#define LOCTEXT_NAMESPACE "FEasyDataTableEditorModule"
const FName FEasyDataTableEditorModule::DataTableEditorAppIdentifier( TEXT( "EasyDataTableEditorApp" ) );
void FEasyDataTableEditorModule::StartupModule()
{
	BuildAssetMenu();
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FEasyDataTableEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

TSharedRef<IEasyDataTableEditor>  FEasyDataTableEditorModule::CreateDataTableEditor(const EToolkitMode::Type Mode,
	const TSharedPtr<class IToolkitHost>& InitToolkitHost, UDataTable* Table)
{
	if (Cast<UCompositeDataTable>(Table) != nullptr)
	{
		return CreateCompositeDataTableEditor(Mode, InitToolkitHost, Table);
	}

	return CreateStandardDataTableEditor(Mode, InitToolkitHost, Table);
}

void FEasyDataTableEditorModule::BuildAssetMenu()
{
	//UDataTable
	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UDataTable::StaticClass());
	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
	Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		InSection.AddMenuEntry(
			"OpenEasyDataTableEditor",
			LOCTEXT("EditorModule", "Open with EasyDataTableEditor"),
			LOCTEXT("EditorModule", "Open with EasyDataTableEditor to edit asset."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
			FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
			{
				const UContentBrowserAssetContextMenuContext* Context = InContext.FindContext<UContentBrowserAssetContextMenuContext>();
				if (!Context)return;
				if(Context->SelectedAssets.IsEmpty())return;
				FEasyDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FEasyDataTableEditorModule>("EasyDataTableEditor");
				for(auto&iter:Context->SelectedAssets)
				{
					if (UDataTable* Table = Cast<UDataTable>(iter.GetAsset()))
					{
						DataTableEditorModule.CreateDataTableEditor(EToolkitMode::Type::Standalone, nullptr, Table);
					}
				}
			})
		);
	}));
}

TSharedRef<IEasyDataTableEditor>  FEasyDataTableEditorModule::CreateStandardDataTableEditor(const EToolkitMode::Type Mode,
	const TSharedPtr<class IToolkitHost>& InitToolkitHost, UDataTable* Table)
{
	TSharedRef< FEasyDataTableEditor > NewDataTableEditor( new FEasyDataTableEditor() );
	NewDataTableEditor->InitDataTableEditor( Mode, InitToolkitHost, Table );
	return NewDataTableEditor;
}

TSharedRef<IEasyDataTableEditor> FEasyDataTableEditorModule::CreateCompositeDataTableEditor(const EToolkitMode::Type Mode,
	const TSharedPtr<class IToolkitHost>& InitToolkitHost, UDataTable* Table)
{
	TSharedRef< FEasyCompositeDataTableEditor > NewDataTableEditor(new FEasyCompositeDataTableEditor());
	NewDataTableEditor->InitDataTableEditor(Mode, InitToolkitHost, Table);
	return NewDataTableEditor;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEasyDataTableEditorModule, EasyDataTableEditor)