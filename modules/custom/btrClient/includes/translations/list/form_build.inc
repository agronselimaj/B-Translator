<?php
/**
 * @file
 * The function btrClient_translate_form() builds a render array
 * for the strings and their corresponding translations/suggestions.
 * The edit and submit functionality will be available only when suitable.
 */

/**
 * Form callback: List strings and the corresponding translations/suggestions.
 *
 * @param $form_state
 *   The form state array.
 * @param $string
 *   A multi-dimentional associative array of the string and the corresponding
 *   translations that are to be rendered.
 * @param $lng
 *   The language code of the translations.
 * @return
 *   A render array of the page.
 */
function btrClient_translate_form($form, &$form_state, $strings, $lng) {
  $pager = theme('pager', array('tags' => NULL, 'element' => 0));
  $form = array(
    'target' => array(
      '#type' => 'value',
      '#value' => $_GET,
    ),
    'langcode' => array(
      '#type' => 'value',
      '#value' => $lng,
    ),
    'pager_top' => array(
      '#weight' => -10,
      '#markup' => $pager,
    ),
    'strings' => array(
      '#tree' => TRUE,
      '#theme' => 'btrClient_translate_table',
      '#lng' => $lng,
    ),

    'buttons' => array(
      //'#prefix' => '<div class="filter-submit">',
      //'#suffix' => '</div>',
    ),

    'pager_bottom' => array(
      '#weight' => 10,
      '#markup' => $pager,
    ),
  );

  // don't show the Back button on the search interface
  if ( current_path() != 'translations/search' && isset($_SESSION['previous_url']) ) {
    $form['buttons']['previous'] = array(
      '#type' => 'submit',
      '#value' => t('Previous'),
    );
  }

  $form['buttons']['login'] = array(
    '#type' => 'submit',
    '#value' => t('Login'),
    '#access' => !user_is_logged_in(),
  );

  // The submit buttons will appear only when the user has
  // permissions to submit votes and suggestions.
  $translation_lng = variable_get('btrClient_translation_lng', 'fr');
  $enable_submit = ($lng==$translation_lng) && (user_access('btranslator-vote') || user_access('btranslator-suggest'));
  $form['buttons']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#access' => $enable_submit,
  );

  // don't show the Next buttons on the search interface
  if (current_path() != 'translations/search') {
    $form['buttons']['next'] = array(
      '#type' => 'submit',
      '#value' => t('Next'),
    );
    $form['buttons']['translated'] = array(
      '#type' => 'submit',
      '#value' => t('Translated'),
    );
    $form['buttons']['untranslated'] = array(
      '#type' => 'submit',
      '#value' => t('Untranslated'),
    );
    $form['buttons']['same_project'] = array(
      '#type' => 'checkbox',
      '#title' => t('Same Project'),
      '#value' => isset($_SESSION['same_project']) ? $_SESSION['same_project'] : FALSE,
    );
  }

  // Fill in string values for the editor.
  foreach ($strings as $sguid => $string) {
    $form['strings'][$sguid] = _btrClient_translate_form_item($form_state, $string, $lng);
    /*
    // TODO: Display the number of comments for each string.
    $form['strings'][$sguid]['disqus'] = array(
      '#theme' => 'links',
      '#links' => array('disqus_comments_num' => array(
      'title' => t('Comments'),
      'href' => "translations/$lng/$sguid",
      'fragment' => 'disqus_thread',
      'attributes' => array(
      // Identify the node for Disqus with the unique identifier:
      // http://docs.disqus.com/developers/universal/#comment-count
      'data-disqus-identifier' => "translations/$lng/$sguid",
      )
      )),
      '#attributes' => array(
      'class' => array('links', 'inline'),
      ),
      );
      $form['strings'][$sguid]['#attached']['js'][] = drupal_get_path('module', 'disqus') . '/disqus.js';
      $form['strings'][$sguid]['#attached']['js'][] = array(
      'data' => array('disqusComments' => "l10n-$lng"),
      'type' => 'setting',
    );
    */
  }

  if (sizeof($strings)==1) {

    // set the page title (no longer than 20 chars)
    $title = btrClient_shorten($string['string'], 20);
    drupal_set_title(t('String: !string', array('!string' => $title),
        array('context' => 'set the page title')));

    $short_string = btrClient_shorten($string['string'], 50);
    $url = url("translations/$lng/$sguid", array('absolute' => TRUE));

    // define the sharethis form element
    $data_options = sharethis_get_options_array();
    $form['sharethis'] = array(
      '#tag' => 'div', // Wrap it in a div.
      '#type' => 'html_tag',
      '#attributes' => array('class' => 'sharethis-buttons'),
      '#value' => sharethis_get_button_HTML($data_options, $path=$url, "String: $short_string"),
      '#weight' => 100,
    );

    // define the disqus form element
    $form['disqus'] = array(
      '#type' => 'disqus',
      '#disqus' => array(
        'domain' => variable_get('disqus_domain', 'dev-btranslator'),
        'status' => TRUE,
        'url' => $url,
        'title' => $short_string,
        'identifier' => "translations/$lng/$sguid",
        'developer' => variable_get('disqus_developer', '1'),
      ),
      '#weight' => 101,
    );
  }

  //dpm($form, '$form');  //debug
  return $form;
}

/**
 * Creates the form fragment for a source string and its translations.
 */
function _btrClient_translate_form_item(&$form_state, $string, $lng) {

  $string['string'] = btrClient_unpack_string($string['string']);

  $form = array(
    '#string' => $string,
    '#langcode' => $lng,
    'source' => array(
      'string' => array('#markup' => _btrClient_translate_render_textarray($string['string'])),
    ),
  );

  if (user_access('btranslator-suggest')) {
    $form['source']['edit'] = array(
      '#markup' => t('Translate'),
      '#prefix' => '<label title="' . t('Translate') . '">',
      '#suffix' => '</label>',
    );
  }

  // Add alternative strings (in other languages).
  /*******
   * TODO: Improve alternatives in other languages.
   $arr_items = array();
   foreach ($string['alternatives'] as $langcode => $string) {
   $string = btrClient_unpack_string($string);
   $rendered_string = _btrClient_translate_render_textarray($string);
   $arr_items = array(
   '#markup' => "<strong>$langcode:</strong><label class='l10n-string'>$rendered_string</label>",
   );
   }
   $form['alternatives'] = array(
   '#theme' => 'item_list',
   '#title' => t('Alternatives in other languages:'),
   '#items' => $arr_items,
   '#prefix' => '<div class="l10n-usage">',
   '#suffix' => '</div>',
   );
  */

  //---------------- Add translations. --------------------

  // Translations are stored in an array.
  $translations = $string['translations'];

  foreach ($translations as $tguid => $translation) {
    // Add the translation to the list.
    $form[$tguid] = _btrClient_translate_translation($translation, $string['sguid'], $lng);
  }

  // If the user may add new suggestions, display a textarea.
  if (user_access('btranslator-suggest')) {
    $textarea = _btrClient_translate_translation_textarea($string, $lng);
    $form[$textarea['tguid']] = _btrClient_translate_translation($textarea, $string['sguid'], $lng);
  }

  return $form;
}

/**
 * Build mock object for new textarea.
 */
function _btrClient_translate_translation_textarea($string, $lng) {
  global $user;

  // Fill in with as many items as required. If the source was plural, we
  // need to fill in with a number adequate for this language.
  $languages = btr_get_languages();
  $nplurals = $languages[$lng]['plurals'];
  $arr_translations = array_fill(0, count($string['string']) > 1 ? $nplurals : 1, '');
  $translation = implode("\0", $arr_translations);

  return array(
    'sguid' => $string['sguid'],
    'tguid' => 'new',
    'lng' => $lng,
    'translation' => $translation,
    'author' => NULL,
    'uid' => $user->uid,
    'count' => '0',
    'votes' => array(),
  );
}

/**
 * Creates the form fragment for a translated string.
 */
function _btrClient_translate_translation($translation, $string_sguid, $lng) {

  global $user;

  $translation['translation'] = btrClient_unpack_string($translation['translation']);

  $is_own = ($user->uid && ($user->name == $translation['author']));
  $is_approved = ($user->uid && in_array($user->name, array_keys($translation['votes'])));
  $is_new = ($translation['tguid'] == 'new');
  $may_moderate = ($is_own or user_access('btranslator-resolve'));

  $form = array(
    '#theme' => 'btrClient_translate_translation',
    'original' => array(
      '#type' => 'value',
      '#value' => $translation,
    ),
  );

  // The 'approved' radio/checkbox is used to pick the approved (voted/liked) translation(s).
  $voting_mode = variable_get('btr_voting_mode', 'single');
  $type = ($voting_mode == 'single') ? 'radio' : 'checkbox';
  $form['approved'] = array(
    '#type' => $type,
    '#theme' => 'btrClient_translate_radio',
    '#theme_wrappers' => array(),
    '#title' => _btrClient_translate_render_textarray($translation['translation'], $is_new ? t('(empty)') : FALSE),
    '#return_value' => $translation['tguid'],
    '#default_value' => $is_approved ? $translation['tguid'] : '',
    '#disabled' => !user_access('btranslator-vote'),
    '#attributes' => array('class' => array('selector')),
  );
  if ($voting_mode == 'single') {
    $form['approved']['#checked'] = $is_approved;
    $form['approved']['#parents'] = array('strings', $string_sguid, 'approved');
  }

  if ($may_moderate && !$is_new) {
    $form['declined'] = array(
      '#type' => 'checkbox',
      '#title' => t('Decline'),
      '#default_value' => FALSE,
    );
  }

  if ($is_new) {
    // Fill in with as many textareas as required to enter translation
    // for this string.
    $form['value'] = array_fill(0, count($translation['translation']), array(
                       '#type' => 'textarea',
                       '#cols' => 60,
                       '#rows' => 3,
                       '#default_value' => t('<New translation>'),
                     ));

  }
  else {
    if (user_access('btranslator-suggest')) {
      $form['edit'] = array(
        '#markup' => t('Edit a copy'),
        '#prefix' => '<label title="' . t('Edit a copy') . '">',
        '#suffix' => '</label>',
      );
    }
    if ($translation['author'] != NULL) {
      $title = btrClient_translate_byline($translation['author'], $translation['uid'], $translation['time']);

      $form['author'] = array(
        '#markup' => $title,
        //'#prefix' => '<div class="l10n-byline"><a href="' . url('translations/details/' . $translation['tguid']) . '" class="l10n-more-link" title="' . t('Show full details for the translation.') . '">',
        //'#suffix' => '</a><div class="l10n-more-info"></div></div>',
      );
    }
    //TODO: Improve displaying of vote count and the voters.
    if ($translation['count'] != '0') {
      $voters = array();
      foreach ($translation['votes'] as $name => $vote) {
        $voters[] = theme('username', array('account' => (object) array('name' => $name, 'uid' => $vote['uid'])));
      }
      $form['votes'] = array(
        '#type' => 'fieldset',
        '#title' => t('Number of Votes: !count', array('!count' => $translation['count'])),
        //'#prefix' => '<div class="l10n-more-info">',
        //'#suffix' => '</div>',
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
        'voters' => array(
          '#markup' => implode(', ', $voters),
        ),
      );
    }
  }

  return $form;
}


/**
 * Generate markup for an unpacked string.
 *
 * @param $textarray
 *   An array of strings as generated by btrClient_unpack_string().
 * @param $empty
 *   Specific data to include as the data to use when empty.
 */
function _btrClient_translate_render_textarray($textarray, $empty = '') {
  // data-empty is a proprietary attribute used in editor.css to be displayed when
  // starting to submit a new suggestion.
  $empty = !empty($empty) ? ' data-empty="' . check_plain($empty) . '"' : '';
  return "<span$empty>" . implode("</span><br /><span$empty>", array_map('check_plain', $textarray)) . '</span>';
}


/**
 * Generates the byline containing meta information about a string.
 */
function btrClient_translate_byline($name, $uid, $time, $medium =-1, $type =-1, $link_user =TRUE) {
  $account = (object) array('name' => $name, 'uid' => $uid);
  $username = ($link_user ? theme('username', array('account' => $account)) : check_plain($name));
  //dpm($username);  //debug
  $params = array(
    '!author' => $uid ? $username : t('uknown user'),
    // Also skip handling time if uid was not specified (for decline entries
    // in the update, which have time for ordering reasons, but no uid).
    '@date' => $time && $uid ? format_date(strtotime($time)) : t('unknown time'),
    '@ago' => $time ? t('@time ago', array('@time' => format_interval(REQUEST_TIME - strtotime($time)))) : t('no time record available'),
  );

  return t('by !author <span title="@ago">on @date</span>', $params);
}

/**
 * From the given (possibly long) string, returns a short string
 * of the given length (that can be suitable for title, subject, etc.)
 */
function btrClient_shorten($string, $length) {
  $str = strip_tags(str_replace("\n", ' ', $string));
  if (strlen($str) > $length) {
    $str = substr($str, 0, strrpos(substr($str, 0, $length - 3), ' '));
    $str .= '...';
  }
  $str = utf8_decode($str);
  $str = htmlentities($str);
  return $str;
}
