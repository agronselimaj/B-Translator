<?php
/**
 * @file
 * btr_btrClient.features.inc
 */

/**
 * Implements hook_ctools_plugin_api().
 */
function btr_btrClient_ctools_plugin_api() {
  list($module, $api) = func_get_args();
  if ($module == "strongarm" && $api == "strongarm") {
    return array("version" => "1");
  }
}

/**
 * Implements hook_node_info().
 */
function btr_btrClient_node_info() {
  $items = array(
    'projects' => array(
      'name' => t('Translation Projects'),
      'base' => 'node_content',
      'description' => t('Translating a software project.'),
      'has_title' => '1',
      'title_label' => t('Project Title'),
      'help' => '',
    ),
  );
  return $items;
}
